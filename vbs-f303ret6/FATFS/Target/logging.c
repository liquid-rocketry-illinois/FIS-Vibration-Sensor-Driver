
#include "logging.h"
#include "fatfs.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include "logging.h"

extern uint32_t SystemCoreClock;


#pragma pack(push, 1)
typedef struct
{
    uint16_t magic;
    uint16_t seq; /* increments per block */
    uint32_t t_us; /* microsecond timestamp */
    int16_t xyz[84][3];
} LogBlock;
#pragma pack(pop)

#define SAMPLES_PER_BLOCK 84
#define LOG_MAGIC 0xADC5
#define RING_BUFFER_ENABLED 0								// Toggle the ring buffer ON (1) and OFF (0) here
#define RING_BLOCKS 32										// ring buffer size
#define SYNC_EVERY_N_WRITES 25								// 5 seconds of data between f_sync calls to bound data loss on power-fail
#define RING_COUNT() ((uint16_t)(ring_head - ring_tail))	//

static LogBlock ring_buf[RING_BLOCKS];
static volatile uint16_t ring_head;		// written only by ISR (interrupt service routine)
static volatile uint16_t ring_tail;		// written only by main
static volatile uint16_t ring_size = 0;

static LogBlock current_block;
static uint8_t current_sample_count = 0;
static uint16_t seq_counter = 0;

static FIL log_file;
static uint32_t writes_since_sync = 0;
static uint16_t overflow_count = 0;

static uint8_t logging_active = 0;		// global flag to indicate if

/* timestamp with Cortex-M4 DWT cycle counter, avoids needing a hardware timer (hopefully) */
static void TIMESTAMP_INIT(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t TIMESTAMP_US(void)
{
    /* SystemCoreClock is 8 MHz per SystemClock_Config (HSI direct and no PLL) */
    return DWT->CYCCNT / (SystemCoreClock / 1000000UL);
}

/**
  *	@brief	Pushes a block to the ring buffer
  *
  * @param  blk		Pointer to block of memory to push to ring buffer
  */
static uint8_t RING_PUSH(const LogBlock *blk)
{
	if (RING_COUNT() >= RING_BLOCKS) { overflow_count++; return 0; }
	ring_buf[ring_head % RING_BLOCKS] = *blk;
	__DMB();                      /* data visible before head moves */
	ring_head++;
	return 1;
}

/**
  *	@brief	Pops a block from the ring buffer
  *
  * @param  out		Pointer to block of memory to pop from buffer into
  */
static uint8_t RING_POP(LogBlock *out)
{
	if (RING_COUNT() == 0) return 0;
	*out = ring_buf[ring_tail % RING_BLOCKS];
	__DMB();
	ring_tail++;
	return 1;
}

/* Incrementing filenames as log000.bin, log001.bin, etc */
static uint8_t OPEN_NEXT_FILE(void)
{
    char name[16];
    FILINFO fno;

    for (uint16_t i = 0; i < 1000; i++)
    {
        snprintf(name, sizeof(name), "log%03u.bin", (unsigned)i);
        if (f_stat(name, &fno) == FR_NO_FILE)
        {
            FRESULT fr = f_open(&log_file, name, FA_CREATE_NEW | FA_WRITE);
            if (fr != FR_OK)
            {
                printf("FAIL: f_open(%s) returned %d\r\n", name, fr);
                return 0;
            }
            printf("Logging to %s\r\n", name);
            return 1;
        }
    }
    printf("FAIL: no free filename slot (1000 logs already made)\r\n");
    /* this surely will not happen for a while, put the except code here just in case */
    return 0;
}

void LOGGING_INIT(void)
{
    /* Initialize FatFs and the timestamp system */
    static FATFS fs;

    TIMESTAMP_INIT();

    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK)
    {
        printf("FAIL: f_mount returned %d, logging disabled\r\n", fr);
        return;
    }

    if (!OPEN_NEXT_FILE()) {return;}

    /* Preallocation of a FAT table to possibly prevent stalling during ring-buffer pop */
    /*
    fr = f_expand(&log_file, 64UL * 1024 * 1024, 1);
    if (fr != FR_OK)
    {
        printf("WARN: f_expand returned %d, logging will continue but write stall may occur\r\n", fr);
    }
    */

    /* refresh all of these variables */
    current_sample_count = 0;
    seq_counter = 0;
    writes_since_sync = 0;
	ring_head = ring_tail = 0;	// refresh ring buffer
	logging_active = 1;
}

void LOGGING_ADD_SAMPLES(const uint8_t *raw, uint8_t entries)
{
	if (!logging_active) return;
    for (uint8_t i = 0; i < entries; i++)
    {
        const uint8_t *s = &raw[i * 6];

        if (current_sample_count == 0)
        {
            /* stamp block with timestamp of its first sample */
            current_block.t_us = TIMESTAMP_US();
        }

        /* shift the high byte up, keep the low byte down, and then stitch the two bytes together (little-endian data stream) */
        current_block.xyz[current_sample_count][0] = (int16_t)(s[1] << 8 | s[0]);
        current_block.xyz[current_sample_count][1] = (int16_t)(s[3] << 8 | s[2]);
        current_block.xyz[current_sample_count][2] = (int16_t)(s[5] << 8 | s[4]);
        current_sample_count++;

        if (current_sample_count >= SAMPLES_PER_BLOCK)
        {
            current_block.magic = LOG_MAGIC;
            current_block.seq = seq_counter++;

        /* preprocessor directive, if statement won't occur during runtime */
#if RING_BUFFER_ENABLED
        RING_PUSH(&current_block);
#else
        {
            uint32_t t0 = HAL_GetTick();
            UINT bw;
            FRESULT fr = f_write(&log_file, &current_block, sizeof(current_block), &bw);
            uint32_t elapsed_ms = HAL_GetTick() - t0;

            printf("%lu ms, fr=%d, %u/%uB, s=%u\r\n",
                (unsigned long)elapsed_ms, fr, bw, (unsigned)sizeof(current_block), current_block.seq);
            if (fr == FR_OK && bw == sizeof(current_block))
            {
                if (++writes_since_sync >= SYNC_EVERY_N_WRITES)
                {
                    f_sync(&log_file);
                    writes_since_sync = 0;
                }
            }
        }
#endif
        current_sample_count = 0;
        }
    }
}

void LOGGING_TASK(void)
{
	if (!logging_active) return;
    /* preprocessor directive, if statement won't occur during runtime */
#if RING_BUFFER_ENABLED
    LogBlock blk;
    if (!RING_POP(&blk)) return; /* if the pop failed, exit function early */

    UINT bw;
    FRESULT fr = f_write(&log_file, &blk, sizeof(blk), &bw);
    if (fr != FR_OK || bw != sizeof(blk))
    {
        printf("FAIL: f_write returned %d, wrote %u/%u bytes\r\n", fr, bw, (unsigned)sizeof(blk));
        return;
    }

    if (++writes_since_sync >= SYNC_EVERY_N_WRITES)
    {
        f_sync(&log_file);
        writes_since_sync = 0;
    }
#else
    /* nothing queued for this function to drain if the ring buffer is disabled*/
#endif
}

void LOGGING_END(void)
{
    /* make sure the log file closes safely */
    f_truncate(&log_file);
    f_close(&log_file);
}

uint32_t LOGGING_GET_OVERFLOW_COUNT(void)
{
    return overflow_count;
}
