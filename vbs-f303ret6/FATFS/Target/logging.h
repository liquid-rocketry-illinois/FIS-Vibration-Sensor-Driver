#ifndef VBS_F303RET6_LOGGING_H
#define VBS_F303RET6_LOGGING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void LOGGING_INIT(void);

void LOGGING_ADD_SAMPLES(const uint8_t *raw, uint8_t entries);

void LOGGING_TASK(void);

void LOGGING_END(void);

uint32_t LOGGING_GET_OVERFLOW_COUNT(void);
uint32_t LOGGING_GET_BLOCKS_WRITTEN(void);
uint8_t  LOGGING_IS_ACTIVE(void);

#ifdef __cplusplus
}
#endif

#endif //VBS_F303RET6_LOGGING_H