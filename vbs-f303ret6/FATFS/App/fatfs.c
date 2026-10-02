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

/* Isolates SD-over-SPI driver (disk_write for diskio.c) from any FatFS layer activity */
void Diskio_Test(void)
{
  printf("\r\nRaw Driver Test\r\n");

  /* Does the disk initialize in the first place? */
  DSTATUS stat = disk_initialize(0);
  if (stat & STA_NOINIT)
  {
    printf("FAIL: disk_initialize() left STA_NOINIT set to (0x%02X)\r\n", stat);
    return;
  }
  printf("disk_initialize() OK (status = 0x%02X)\r\n", stat);

  /* Are we able to read the sectors in the disk? */
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

  /* Can we read the first sector? (Test) */
  static BYTE sector_buffer[512];
  diskread = disk_read(0, sector_buffer, 0, 1);
  if (diskread != RES_OK)
  {
    printf("FAIL: disk_read(sector 0) returned %d\r\n", diskread);
    return;
  }
  printf("disk_read(sector 0) OK\r\n");

  /* Do the boot signatures at the end of the sector look okay? */
  if (sector_buffer[510] == 0x55 && sector_buffer[511] == 0xAA)
  {
    printf("PASS: Sector 0 had valid boot signature\r\n");
  }
  else
  {
    printf("FAIL: No 0x55AA signature found (card may be unformatted)\r\n");
  }

  printf("Disk Test Complete");
}

/* Tests FatFS interface itself */
void FATFS_Test(void)
{
  static FATFS fs;
  static FIL fil;
  FRESULT fres;
  UINT bw, br;

  printf("\r\n FatFS Test \r\n");

  /* Can FatFS successfully mount the filesystem on the SD card? */
  fres = f_mount(&fs, "0:", 1);
  if (fres != FR_OK)
  {
    printf("FAIL: f_mount() returned FResult %d\r\n", fres);
    /* If the card is unformatted, try f_mkfs() first or reformat as FAT32 on a PC manually */
    return;
  }
  printf("f_mount successful!\r\n");
  const char wbuf[] = "SD card Test (from STM32)\r\n";

  /* Can FatFS create a file on the SD card? */
  fres = f_open(&fil, "test.txt", FA_CREATE_ALWAYS | FA_WRITE);
  if (fres != FR_OK)
  {
    printf("FAIL: f_open (create) returned FRESULT %d\r\n", fres);
    return;
  }

  /* Can FatFS write to a file on the SD card? */
  fres = f_write(&fil, wbuf, strlen(wbuf), &bw);
  f_close(&fil);
  if (fres != FR_OK || bw != strlen(wbuf))
  {
    printf("FAIL: f_write returned %d, wrote %u/%u bytes\r\n", fres, bw, (unsigned)strlen(wbuf));
    return;
  }
  printf("Wrote %u bytes to test.txt\r\n", bw);

  /* Can FatFS reopen the existing file and read data correctly? */
  char rbuf[64] = {0};
  fres = f_open(&fil, "test.txt", FA_READ);
  if (fres != FR_OK)
  {
    printf("FAIL: f_open (read) returned FRESULT %d\r\n", fres);
    return;
  }
  fres = f_read(&fil, rbuf, sizeof(rbuf) - 1, &br);
  f_close(&fil);
  if (fres != FR_OK)
  {
    printf("FAIL: f_read() returned FRESULT %d\r\n", fres);
    return;
  }

  /* Compare what was input into the file to what we got back */
  rbuf[br] = '\0';
  printf("Read back %u bytes: \"%s\"", br, rbuf);
  if (strncmp(wbuf, rbuf, strlen(wbuf)) == 0)
  {
    printf("PASS: readback matches that was written\r\n");
  }
  else
  {
    printf("FAIL: readback doesn't match what was written\r\n");
  }

  /* Check if free-space reporting works */
  FATFS *pfs;
  DWORD free_clusters;
  fres = f_getfree("", &free_clusters, &pfs);
  if (fres == FR_OK)
  {
    DWORD free_kb = (free_clusters * pfs->csize) / 2;
    printf("Free Space: %lu KB \r\n", free_kb);
  }
  else
  {
    printf("FAIL: f_getfree() returned FRESULT %d\r\n", fres);
  }

  printf("Test Complete");

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