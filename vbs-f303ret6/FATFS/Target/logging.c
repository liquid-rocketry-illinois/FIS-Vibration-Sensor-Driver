#include "logging.h"
#include "fatfs.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

extern uint32_t SystemCoreClock;

#pragma pack(push, 1)
typedef struct
{
    uint16_t magic;
    uint16_t seq; // increments per block
    uint32_t t_us; // microsecond timestamp of the block's first sample
    int16_t xyz[84][3];
} LogBlock;
#pragma pack(pop)

#define SAMPLES_PER_BLOCK 84
#define LOG_MAGIC 0xADC5

#define RING_BUFFER_CAPACITY 2688   /* 2688 samples * 6 bytes = 16KB */

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} AccelSample;

typedef struct {
    AccelSample samples[RING_BUFFER_CAPACITY];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint32_t overflow_count;
} RingBuffer;

static RingBuffer accel_buffer = {0};

static FIL log_file;
static uint32_t writes_since_sync = 0;
static uint16_t seq_counter = 0;

// Block currently being assembled from popped samples, on the TASK/write side
static LogBlock task_block;
static uint8_t task_sample_count = 0;

// 5 seconds of data between f_sync calls to bound data loss on power-fail
#define SYNC_EVERY_N_WRITES 25

// DWT-based microsecond timestamp
static void TIMESTAMP_INIT(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t TIMESTAMP_US(void)
{
    return DWT->CYCCNT / (SystemCoreClock / 1000000UL);
}

/**
 * @brief Add one accelerometer sample to the software ring buffer.
 * @return true if stored, false if the buffer was full.
 *
 * Called from FIFO_DRAIN_TO_LOG() inside the EXTI ISR.
 * When full, this drops the newest sample and counts the overflow.
 */
static bool RingBuffer_Push(RingBuffer *rb, AccelSample sample)
{
    if (rb->count >= RING_BUFFER_CAPACITY) {
        rb->overflow_count++;
        return false;
    }
    rb->samples[rb->head] = sample;
    rb->head = (uint16_t)((rb->head + 1U) % RING_BUFFER_CAPACITY);
    rb->count++;
    return true;
}

/**
 * @brief Remove the oldest sample from the ring buffer.
 * @return true if a sample was returned, false if the buffer was empty.
 *
 * See RingBuffer_Push() for more info.
 */
static bool RingBuffer_Pop(RingBuffer *rb, AccelSample *sample)
{
    if (rb->count == 0U || sample == NULL) {
        return false;
    }
    *sample = rb->samples[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1U) % RING_BUFFER_CAPACITY);
    rb->count--;
    return true;
}

// Incrementing filenames as log000.bin, log001.bin, etc
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
    return 0;
}

void LOGGING_INIT(void)
{
    static FATFS fs;

    TIMESTAMP_INIT();

    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK)
    {
        printf("FAIL: f_mount returned %d, logging disabled\r\n", fr);
        return;
    }

    if (!OPEN_NEXT_FILE()) { return; }

    // Preallocation, avoids FatFs updating the FAT table mid-log every time the file grows
    // Might prevent write stalls
    /*
    fr = f_expand(&log_file, 64UL * 1024 * 1024, 1);
    if (fr != FR_OK)
    {
        printf("WARN: f_expand returned %d, logging will continue but write stall may occur\r\n", fr);
    }
    */

    task_sample_count = 0;
    seq_counter = 0;
    writes_since_sync = 0;
    accel_buffer.head = accel_buffer.tail = accel_buffer.count = 0;
    accel_buffer.overflow_count = 0;
}

/**
 * @brief Decode one FIFO-drain's worth of raw 6-byte samples and push each one into the ring buffer.
 *
 */
void LOGGING_ADD_SAMPLES(const uint8_t *raw, uint8_t entries)
{
    for (uint8_t i = 0; i < entries; i++)
    {
        const uint8_t *s = &raw[i * 6];

        AccelSample sample;
        sample.x = (int16_t)(s[1] << 8 | s[0]);
        sample.y = (int16_t)(s[3] << 8 | s[2]);
        sample.z = (int16_t)(s[5] << 8 | s[4]);

        RingBuffer_Push(&accel_buffer, sample);  // overflow_count self-tracks on failure
    }
}

/**
 * @brief Called from the main loop. Pops available samples, assembles them into a 512-byte LogBlock,
 *        and writes the block to SD once it's full.
 */
void LOGGING_TASK(void)
{
    AccelSample sample;

    while (task_sample_count < SAMPLES_PER_BLOCK && RingBuffer_Pop(&accel_buffer, &sample))
    {
        if (task_sample_count == 0) {
            task_block.t_us = TIMESTAMP_US();
        }
        task_block.xyz[task_sample_count][0] = sample.x;
        task_block.xyz[task_sample_count][1] = sample.y;
        task_block.xyz[task_sample_count][2] = sample.z;
        task_sample_count++;
    }

    if (task_sample_count < SAMPLES_PER_BLOCK) {
        return;   // not enough samples yet, try again next call
    }

    task_block.magic = LOG_MAGIC;
    task_block.seq = seq_counter++;

    UINT bw;
    FRESULT fr = f_write(&log_file, &task_block, sizeof(task_block), &bw);
    if (fr != FR_OK || bw != sizeof(task_block))
    {
        printf("FAIL: f_write returned %d, wrote %u/%u bytes\r\n", fr, bw, (unsigned)sizeof(task_block));
        task_sample_count = 0;   // drop this block rather than retry it forever
        return;
    }

    task_sample_count = 0;

    if (++writes_since_sync >= SYNC_EVERY_N_WRITES)
    {
        f_sync(&log_file);
        writes_since_sync = 0;
    }
}

void LOGGING_END(void)
{
    f_truncate(&log_file);
    f_close(&log_file);
}

uint32_t LOGGING_GET_OVERFLOW_COUNT(void)
{
    return accel_buffer.overflow_count;
}