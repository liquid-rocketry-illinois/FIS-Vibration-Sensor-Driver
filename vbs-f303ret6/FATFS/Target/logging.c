
#include "logging.h"
#include "fatfs.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

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

/* Toggle the ring buffer on (1) and off (0) here*/
#define RING_BUFFER_ENABLED 1
#define RING_BLOCKS 32 /* ring buffer size */

static LogBlock ring_buf[RING_BLOCKS];
static volatile uint16_t ring_head = 0; /* next slot to write */
static volatile uint16_t ring_tail = 0; /* next slot to read */
static volatile uint16_t ring_size = 0;

static LogBlock current_block;
static uint8_t current_sample_count = 0;
static uint16_t seq_counter = 0;

static FIL log_file;
static uint32_t writes_since_sync = 0;
static uint16_t overflow_count = 0;

/* 5 seconds of data between f_sync calls to bound data loss on power-fail */
#define SYNC_EVERY_N_WRITES 25

/* timestamp with Cortex-M4 DWT cycle counter, avoids needing a hardware timer (hopefully) */
static void Timestamp_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t Timestamp_US(void)
{
    /* SystemCoreClock is 8 MHz per SystemClock_Config (HSI direct and no PLL) */
    return DWT->CYCCNT / (SystemCoreClock / 1000000UL);
}

/* Ring bugger push and pop functions, single threaded */
static uint8_t Ring_Push(const LogBlock *blk)
{
    if (ring_size >= RING_BLOCKS)
    {
        overflow_count++;
        return 0; /* ring buffer full, so drop the block entirely */
    }
    ring_buf[ring_head] = *blk;
    ring_head = (uint16_t)((ring_head + 1) % RING_BLOCKS);
    ring_size++;
    return 1;
}

static uint8_t Ring_Pop(LogBlock *out)
{
    if (ring_size == 0) return 0;
    *out = ring_buf[ring_tail];
    ring_tail = (uint16_t)((ring_tail + 1) % RING_BLOCKS);
    ring_size--;
    return 1;
}

/* Incrementing filenames as log000.bin, log001.bin, etc */
static uint8_t Open_Next_File(void)
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

void Logging_Init(void)
{
    /* Initialize FatFs and the timestamp system */
    static FATFS fs;

    Timestamp_Init();

    FRESULT fr = f_mount(&fs, "", 1);
}
