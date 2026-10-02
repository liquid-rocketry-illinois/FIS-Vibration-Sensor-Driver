/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   SD-over-SPI FatFs driver for hspi3 (SCK=PC10, MOSI=PC12,
  *          MISO=PC11, CS=PA1)
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include <stdio.h>
#include "main.h"   /* for hspi3 handle + GPIO defines */

extern SPI_HandleTypeDef hspi3;

/* SD card CS -- adjust port/pin if yours differ */
#define SD_CS_PORT   GPIOA
#define SD_CS_PIN    GPIO_PIN_1
#define SD_CS_LOW()  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET)
#define SD_CS_HIGH() HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET)

/* SD SPI-mode commands */
#define CMD0    (0)     /* GO_IDLE_STATE                       */
#define CMD8    (8)     /* SEND_IF_COND                        */
#define CMD9    (9)     /* SEND_CSD                             */
#define CMD17   (17)    /* READ_SINGLE_BLOCK                    */
#define CMD24   (24)    /* WRITE_BLOCK                          */
#define CMD55   (55)    /* APP_CMD                               */
#define ACMD41  (41)    /* SD_SEND_OP_COND                       */
#define CMD58   (58)    /* READ_OCR                              */

#define DATA_TOKEN_SINGLE  (0xFE)   /* start token, single block read/write */
#define DATA_RESP_MASK     (0x1F)
#define DATA_RESP_ACCEPTED (0x05)

/* 0 = unknown/failed, 1 = SDSC (byte addressing), 2 = SDHC/SDXC (block addressing) */
static uint8_t CardType = 0;

static volatile DSTATUS Stat = STA_NOINIT;

DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif
};

static uint8_t SD_SPI_TxRx(uint8_t data)
{
    uint8_t rx = 0xFF;
    HAL_SPI_TransmitReceive(&hspi3, &data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

static void SD_SPI_SetSlow(void)
{
    __HAL_SPI_DISABLE(&hspi3);
    hspi3.Instance->CR1 &= ~SPI_CR1_BR;
    hspi3.Instance->CR1 |= SPI_BAUDRATEPRESCALER_256;
    __HAL_SPI_ENABLE(&hspi3);
}

static void SD_SPI_SetFast(void)
{
    __HAL_SPI_DISABLE(&hspi3);
    hspi3.Instance->CR1 &= ~SPI_CR1_BR;
    hspi3.Instance->CR1 |= SPI_BAUDRATEPRESCALER_4;
    __HAL_SPI_ENABLE(&hspi3);
}

/* Polls until the card returns 0xFF (no longer busy) or timeout */
static uint8_t SD_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t res;
    do {
        res = SD_SPI_TxRx(0xFF);
    } while (res != 0xFF && (HAL_GetTick() - start) < timeout_ms);
    return res;
}

/* Polls until a specific token byte appears */
static uint8_t SD_WaitToken(uint8_t token, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t r;
    do {
        r = SD_SPI_TxRx(0xFF);
    } while (r != token && (HAL_GetTick() - start) < timeout_ms);
    return r;
}

/* Sends a 6-byte command frame and returns the card's R1 response */
static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg)
{
    uint8_t crc = 0x01;
    if (cmd == CMD0) crc = 0x95;
    if (cmd == CMD8) crc = 0x87;

    SD_WaitReady(500);

    SD_SPI_TxRx(0x40 | cmd);
    SD_SPI_TxRx((uint8_t)(arg >> 24));
    SD_SPI_TxRx((uint8_t)(arg >> 16));
    SD_SPI_TxRx((uint8_t)(arg >> 8));
    SD_SPI_TxRx((uint8_t)arg);
    SD_SPI_TxRx(crc);

    uint8_t res;
    uint8_t retry = 10;
    do {
        res = SD_SPI_TxRx(0xFF);
    } while ((res & 0x80) && --retry);

    return res;
}

/* USER CODE END DECL */


DSTATUS USER_initialize (BYTE pdrv)
{
  /* USER CODE BEGIN INIT */
    uint8_t r1;
    uint8_t cmd8_resp[4] = {0};
    uint8_t ocr[4] = {0};
    uint16_t retry;

    if (pdrv != 0) {
        Stat = STA_NOINIT;
        return Stat;
    }

    SD_CS_HIGH();
    SD_SPI_SetSlow();

    for (uint8_t i = 0; i < 10; i++) SD_SPI_TxRx(0xFF);   /* >=74 clocks, CS high */

    SD_CS_LOW();
    r1 = SD_SendCmd(CMD0, 0);
    SD_CS_HIGH();
    SD_SPI_TxRx(0xFF);

    printf("[SD INIT] CMD0 raw response: 0x%02X\r\n", r1);
    if (r1 != 0x01) {
        Stat = STA_NOINIT;
        return Stat;   /* no response to reset -- check wiring/CS/8-bit frame size */
    }

    SD_CS_LOW();
    r1 = SD_SendCmd(CMD8, 0x1AA);
    if (r1 == 0x01) {
        for (int i = 0; i < 4; i++) cmd8_resp[i] = SD_SPI_TxRx(0xFF);
    }
    SD_CS_HIGH();
    SD_SPI_TxRx(0xFF);

    uint8_t is_v2 = (r1 == 0x01 && cmd8_resp[2] == 0x01 && cmd8_resp[3] == 0xAA);

    retry = 20000;
    do {
        SD_CS_LOW();
        SD_SendCmd(CMD55, 0);
        SD_CS_HIGH();
        SD_SPI_TxRx(0xFF);

        SD_CS_LOW();
        r1 = SD_SendCmd(ACMD41, is_v2 ? 0x40000000UL : 0);
        SD_CS_HIGH();
        SD_SPI_TxRx(0xFF);
    } while (r1 != 0x00 && --retry);

    if (r1 != 0x00) {
        Stat = STA_NOINIT;
        return Stat;   /* card never left idle */
    }

    CardType = 1;
    if (is_v2) {
        SD_CS_LOW();
        r1 = SD_SendCmd(CMD58, 0);
        if (r1 == 0x00) {
            for (int i = 0; i < 4; i++) ocr[i] = SD_SPI_TxRx(0xFF);
            if (ocr[0] & 0x40) CardType = 2;   /* CCS bit and block addressing */
        }
        SD_CS_HIGH();
        SD_SPI_TxRx(0xFF);
    }

    SD_SPI_SetFast();
    Stat = 0;
    return Stat;
  /* USER CODE END INIT */
}

DSTATUS USER_status (BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return Stat;
}

/* Reads Sector(s) */
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
  /* USER CODE BEGIN READ */
    if (pdrv != 0) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    /* SDSC cards are byte-addressed, SDHC/SDXC are block-addressed */
    DWORD addr = (CardType == 2) ? sector : (sector * 512);

    while (count--) {
        SD_CS_LOW();
        uint8_t r1 = SD_SendCmd(CMD17, addr);
        if (r1 != 0x00) {
            SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);
            return RES_ERROR;
        }

        if (SD_WaitToken(DATA_TOKEN_SINGLE, 200) != DATA_TOKEN_SINGLE) {
            SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);
            return RES_ERROR;   /* card never sent the data-start token */
        }

        for (uint16_t i = 0; i < 512; i++) {
            buff[i] = SD_SPI_TxRx(0xFF);
        }
        SD_SPI_TxRx(0xFF);   /* discard CRC (2 bytes) */
        SD_SPI_TxRx(0xFF);

        SD_CS_HIGH();
        SD_SPI_TxRx(0xFF);

        buff += 512;
        addr += (CardType == 2) ? 1 : 512;
    }
    return RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  */
#if _USE_WRITE == 1
DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
  /* USER CODE BEGIN WRITE */
    if (pdrv != 0) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    DWORD addr = (CardType == 2) ? sector : (sector * 512);

    while (count--) {
        SD_CS_LOW();
        uint8_t r1 = SD_SendCmd(CMD24, addr);
        if (r1 != 0x00) {
            SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);
            return RES_ERROR;
        }

        SD_SPI_TxRx(DATA_TOKEN_SINGLE);
        for (uint16_t i = 0; i < 512; i++) {
            SD_SPI_TxRx(buff[i]);
        }
        SD_SPI_TxRx(0xFF);   /* dummy CRC (2 bytes), ignored in SPI mode */
        SD_SPI_TxRx(0xFF);

        uint8_t resp = SD_SPI_TxRx(0xFF);
        if ((resp & DATA_RESP_MASK) != DATA_RESP_ACCEPTED) {
            SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);
            return RES_ERROR;   /* card rejected the block, CRC/write error */
        }

        if (SD_WaitReady(500) != 0xFF) {
            SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);
            return RES_ERROR;   /* card stayed busy past timeout */
        }

        SD_CS_HIGH();
        SD_SPI_TxRx(0xFF);

        buff += 512;
        addr += (CardType == 2) ? 1 : 512;
    }
    return RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
  /* USER CODE BEGIN IOCTL */
    if (pdrv != 0) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    switch (cmd) {

    case CTRL_SYNC:
        SD_CS_LOW();
        {
            DRESULT r = (SD_WaitReady(500) == 0xFF) ? RES_OK : RES_ERROR;
            SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);
            return r;
        }

    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        return RES_OK;

    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;   /* erase block size unknown */
        return RES_OK;

    case GET_SECTOR_COUNT:
    {
        uint8_t csd[16];
        uint8_t r1;

        SD_CS_LOW();
        r1 = SD_SendCmd(CMD9, 0);
        if (r1 != 0x00 || SD_WaitToken(DATA_TOKEN_SINGLE, 200) != DATA_TOKEN_SINGLE) {
            SD_CS_HIGH();
            SD_SPI_TxRx(0xFF);
            return RES_ERROR;
        }
        for (int i = 0; i < 16; i++) csd[i] = SD_SPI_TxRx(0xFF);
        SD_SPI_TxRx(0xFF);   /* discard CRC */
        SD_SPI_TxRx(0xFF);
        SD_CS_HIGH();
        SD_SPI_TxRx(0xFF);

        DWORD sectors;
        if (csd[0] >> 6 == 1) {
            /* CSD version 2.0 (SDHC/SDXC) */
            DWORD c_size = ((DWORD)(csd[7] & 0x3F) << 16) | ((DWORD)csd[8] << 8) | csd[9];
            sectors = (c_size + 1) * 1024;   /* each unit = 512 KB = 1024 sectors */
        } else {
            /* CSD version 1.0 (SDSC) */
            DWORD c_size = ((DWORD)(csd[6] & 0x03) << 10) | ((DWORD)csd[7] << 2) | (csd[8] >> 6);
            DWORD c_size_mult = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
            DWORD read_bl_len = csd[5] & 0x0F;
            DWORD block_count = (c_size + 1) << (c_size_mult + 2);
            DWORD block_len = 1UL << read_bl_len;
            sectors = (block_count * block_len) / 512;
        }
        *(DWORD *)buff = sectors;
        return RES_OK;
    }

    default:
        return RES_PARERR;
    }
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */