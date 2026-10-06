
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