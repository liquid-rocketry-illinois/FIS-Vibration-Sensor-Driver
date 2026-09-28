/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include "fatfs.h"
#include "diskio.h"
#include <stdio.h>
#include <string.h>


uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */
FATFS USERFatFS;    /* File system object for USER logical drive */
FIL USERFile;       /* File object for USER */

/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

void diskio_test(void)
{
  printf("\r\nRaw Driver Test\r\n");

  DSTATUS stat = disk_initialize(0);
  if (stat & STA_NOINIT)
  {
    printf("FAIL: disk_initialize() left STA_NOINIT set to (0x%02X)\r\n", stat);
    return;
  }
  printf("disk_initialize() OK (status = 0x%02X)\r\n", stat);

  DWORD sector_count = 0;
  DRESULT diskread = disk_ioctl(0, GET_SECTOR_COUNT, &sector_count);

  if (diskread == RES_OK)
  {
    printf("Reported sector count: %lu (%lu MB)\r\n", sector_count, sector_count / 1024);
  }
  else
  {
    printf("WARN: disk_ioctl() failed (status = %d)\r\n", diskread);
  }

  /* attempt to read first sector (boot/ */
  static BYTE sector_buffer[512];
  diskread = disk_read(0, sector_buffer, 0, 1);
  if (diskread != RES_OK)
  {
    printf("FAIL: disk_read(sector 0) returned %d\r\n", diskread);
    return;
  }
  printf("disk_read(sector 0) OK\r\n");



}

void MX_FATFS_Init(void)
{
  /*## FatFS: Link the USER driver ###########################*/
  retUSER = FATFS_LinkDriver(&USER_Driver, USERPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  return 0;
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

/* USER CODE END Application */