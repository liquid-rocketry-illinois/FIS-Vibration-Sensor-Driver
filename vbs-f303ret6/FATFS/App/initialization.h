#ifndef INITIALIZATION_H
#define INITIALIZATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Low-level diskio test.
 */
void Diskio_Init(void);

/**
 * @brief  FatFs-level test.
 */
void FatFS_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* INITIALIZATION_H */