#ifndef INITIALIZATION_H
#define INITIALIZATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Low-level diskio test.
 */
void DISKIO_INIT(void);

/**
 * @brief  FatFs-level test.
 */
void FATFS_INIT(void);

#ifdef __cplusplus
}
#endif

#endif /* INITIALIZATION_H */