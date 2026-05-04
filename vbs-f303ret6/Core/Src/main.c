/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DEVID_REG             (0x00)  // R

/*
 *  8-bit registers holding user-set offset adjustment
 *  (2's complement), scale factor 15.6 mb/LSB
 */

#define OFSX_REG              (0x1E)  // R/W
#define OFSY_REG              (0x1F)  // R/W
#define OFSZ_REG              (0x20)  // R/W

/*
 *  Interrupt registers
 */

#define INT_ENABLE_REG        (0x2E)  // R/W
#define INT_MAP_REG           (0x2F)  // R/W
#define INT_SOURCE_REG        (0x30)  // R

/*
 *  FIFO registers
 */

#define FIFO_CTL_REG          (0x38)  // R/W
#define FIFO_STATUS_REG       (0x39)  // R

// FIFO access bitmasks

#define FIFO_BYPASS           (0x00)  // 00xx xxxx
#define FIFO_FIFO             (0x40)  // 01xx xxxx
#define FIFO_STREAM           (0x80)  // 10xx xxxx
#define FIFO_TRIGGER          (0xC0)  // 11xx xxxx

#define FIFO_WATERMARK        (0x10)  // watermark = #16 (interrupt at 16 FIFO entries)

/*
 *  Initialized registers for data formatting and ODR, respectively
 */

#define BW_RATE_REG           (0x2C)  // R/W
#define POWER_CTL_REG         (0x2D)  // R/W
#define DATA_FORMAT_REG       (0x31)  // R/W

/*
 *  Axes raw acceleration data (SEXT 10b 2's comp)
 *  For register Ax (where A is X/Y/Z and x is 0/1), assuming right justify (DATA_FORMAT)
 *  A1: MSB, D[15:8]; D[15:10]: sign extension
 *  A0: LSB, D[7:0]
 */

#define DATA_X0_REG           (0x32)  // R
#define DATA_X1_REG           (0x33)  // R
#define DATA_Y0_REG           (0x34)  // R
#define DATA_Y1_REG           (0x35)  // R
#define DATA_Z0_REG           (0x36)  // R
#define DATA_Z1_REG           (0x37)  // R

// SPI access bitmasks

#define VERIFY                (0xE5)  // ID code 0xE5
#define ADXL345_RW            (0x80)  // Bit 7 (MSB)  R/W'
#define ADXL345_MB            (0x40)  // Bit 6 (MB)   multibyte

/*
 *  ADXL345 CS configurations; CS is active high, pull low for slave access
 *  ADXL1_CS_LOW:   Pulls CS GPIO low (0)
 *  ADXL1_CS_HIGH:  Pulls CS GPIO high (1)
 */

#define ADXL_CS_PORT           GPIOA
#define ADXL_CS_PIN            GPIO_PIN_0

#define ADXL1_CS_LOW()   HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET)
#define ADXL1_CS_HIGH()  HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  *  @brief   Write to one register with a singular SPI transaction
  *
  *  @param   reg   register address
  *  @param   value uint_t value to be written into specified register
  */
void SPI_WRITE(uint8_t reg, uint8_t value)
{
  /*
   *  For write:
   *  tx[1][7] = R/W', tx[1][6] = MB, tx[1][5:0] = Register address bits
   *  tx[2][7:0] = Data bits
   */

  uint8_t tx[2] = {(reg & ~ADXL345_RW), value};
  ADXL1_CS_LOW();
  HAL_SPI_Transmit(&hspi2, tx, 2, HAL_MAX_DELAY);
  ADXL1_CS_HIGH();
}

/**
  *  @brief   Receive a block of data from a singular SPI transaction
  *
  *  @param   reg   first register address
  *  @param   pData pointer to uint_8 transmit data buffer
  *  @param   len   consecutive number of registers to read from, inclusive (i.e., [reg:reg+len-1])
  */
void SPI_WRITE_BURST(uint8_t reg, uint8_t *pData, uint8_t len)
{
  /*
   *  For read:
   *  tx[7] = R/W', tx[6] = MB (1), tx[5:0] = Register address bits
   *  rx[7:0] = Data bits
   */

  uint8_t tx[len];
  tx[0]     = ((reg & ~ADXL345_RW) | ADXL345_MB);
  ADXL1_CS_LOW();
  HAL_SPI_Transmit(&hspi2, tx, len, HAL_MAX_DELAY);
  ADXL1_CS_HIGH();
}

/**
  *  @brief   Read one register from a singular SPI transaction
  *
  *  @param   reg register address
  *  @retval  rx data
  */
uint8_t SPI_READ(uint8_t reg)
{
  /*
   *  For read:
   *  tx[7] = R/W', tx[6] = MB (kept at 0), tx[5:0] = Register address bits
   *  rx[7:0] = Data bits
   */
  uint8_t tx  = (reg | ADXL345_RW);
  uint8_t rx  = 0;
  ADXL1_CS_LOW();
  HAL_SPI_Transmit(&hspi2, &tx, 1, HAL_MAX_DELAY);
  HAL_SPI_Receive(&hspi2, &rx, 1, HAL_MAX_DELAY);
  ADXL1_CS_HIGH();
  return rx;
}

/**
  *  @brief   Receive a block of data from a singular SPI transaction
  *
  *  @param   reg   first register address
  *  @param   pData pointer to uint_8 data buffer
  *  @param   len   consecutive number of registers to read from, inclusive (i.e., [reg:reg+len-1])
  */
void SPI_READ_BURST(uint8_t reg, uint8_t *pData, uint8_t len)
{
  /*
   *  For read:
   *  tx[7] = R/W', tx[6] = MB (1), tx[5:0] = Register address bits
   *  rx[7:0] = Data bits
   */

  uint8_t tx  = (reg | ADXL345_RW | ADXL345_MB);
  ADXL1_CS_LOW();
  HAL_SPI_Transmit(&hspi2, &tx, 1, HAL_MAX_DELAY);
  HAL_SPI_Receive(&hspi2, pData, len, HAL_MAX_DELAY);
  ADXL1_CS_HIGH();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  // Initialization and troubleshooting
  ADXL1_CS_HIGH();          // SPI has CS active LOW, default HIGH

  uint8_t id  = SPI_READ(DEVID_REG);
  if (id == VERIFY) {
    printf("[PASS] Device ID = 0x%02X\r\n", id);
  }
  else {
    printf("[FAIL] Connection to device could not be established = 0x%02X\r\n", id);
    Error_Handler();
  }

  /*
   *  Sets the BW_RATE register (for ODR)
   *  D[7:4]: 0
   *  D[3:0]: RATE BITS (ADXL345 datasheet, Table 7)
   */
  SPI_WRITE(BW_RATE_REG, 0x0F);   // Sets ODR to 3200 Hz (bandwidth 1600 Hz)
  printf("BW RATE: 0x%02X\r\n", SPI_READ(BW_RATE_REG));

  /*
   *  Sets the FIFO_CTL register
   *  D[7:6]: FIFO MODE   = 10    (Stream mode)
   *  D[5]:               = 1     (Trigger event of trigger mode linked to INT2)
   *  D[4:0]:             = 10000 (#16 FIFO entries to trigger watermark interrupt)
   *
   *  0b10110000 > 0x90
   */
  SPI_WRITE(FIFO_CTL_REG, FIFO_STREAM | 0x20 | FIFO_WATERMARK);
  printf("FIFO CTL: 0x%02X\r\n", SPI_READ(FIFO_CTL_REG));   // expected 0xB0

  /*
   *  Sets the INT_ENABLE register
   *  D7:     DATA READY  = 0
   *  D[6:5]: TAP EVENTS    xx
   *  D[4:3]: ACTIVITY      xx
   *  D2:     FREE FALL     x
   *  D1:     WATERMARK   = 1
   *  D0:     OVERRUN     = 0
   *
   *  0b00000011 > 0x02
   */
  SPI_WRITE(INT_ENABLE_REG, 0x02);
  printf("INT ENABLE: 0x%02X\r\n", SPI_READ(INT_ENABLE_REG));

  /*
   *  Sets the INT_MAP register
   *  Same meaning as above, any interrupts from respective
   *  functions (e.g., WATERMARK) is sent to INT1 if = 0,
   *  INT2 if 1.
   *
   *  0b00000011 > 0x02
   */
  SPI_WRITE(INT_MAP_REG, 0x02);
  printf("INT MAP: 0x%02X\r\n", SPI_READ(INT_MAP_REG));

  /*
   *  Sets the DATA_FORMAT register
   *  D7:     SELF TEST   = 0
   *  D6:     SPI         = 0   (Full duplex, 4w)
   *  D5:     INT_INVERT  = 0   (Interrupts active high)
   *  D4:                   0
   *  D3:     FULL_RES    = 1   (4 mg/LSB scale factor)
   *  D2:     JUSTIFY     = 0   (right justify)
   *  D[1:0]: RANGE BITS  = 11  (+/- 16g)
   *
   *  0b00001011 > 0x0B
   */
  SPI_WRITE(DATA_FORMAT_REG, 0x0B);
  printf("DATA FORMAT: 0x%02X\r\n", SPI_READ(DATA_FORMAT_REG));

  /*
   *  Sets the POWER_CTL register
   *  D[7:6]:               00
   *  D5:     LINK        = 0   (inactivity/activity functions concurrent)
   *  D4:     AUTO_SLEEP  = 0   (disable auto-sleep)
   *  D3:     MEASURE     = 1   (enable measurement mode)
   *  D2:     SLEEP       = 0   (disable sleep mode)
   *  D[1:0]: WAKEUP BITS = xx  (frequency of reading during sleep mode, not relevant)
   *
   *  0b00001000 > 0x08
   */
  SPI_WRITE(POWER_CTL_REG, 0x08);
  printf("POWER CTL: 0x%02X\r\n", SPI_READ(POWER_CTL_REG));
  HAL_Delay(5000); // 5-second delay for checking register return values

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    const uint16_t odr    = 3200;                   //  output data rate
    uint8_t fifo_status   = SPI_READ(FIFO_STATUS_REG);
    uint8_t fifo_entries  = (fifo_status & 0x3F);   //  bitmask of 6 LSB

    if (fifo_entries >= FIFO_WATERMARK) {
      uint8_t raw[6];
      SPI_READ_BURST(DATA_X0_REG, raw, 6);

      int16_t x   = (int16_t)(raw[1]<<8 | raw[0]);
      int16_t y   = (int16_t)(raw[3]<<8 | raw[2]);
      int16_t z   = (int16_t)(raw[5]<<8 | raw[4]);

      printf("RAW DATA\r\n"
             "X: %d\r\n"
             "Y: %d\r\n"
             "Z: %d\r\n",
             x, y, z);
    }
    else
    {
      printf("[WAIT] Polling threshold not reached.\r\n");
    }
    HAL_Delay(1000/odr);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /*
   *  Note that APB1 clock runs at 8 MHz
   *  with the current clock configuration.
   *
   *  SPI2 BaudRatePrescaler is 2:
   *  8 MHz / 2 = 4 MHz SPI clock speed
   *
   *  The ADXL345 has a max SPI clock speed of 5 MHz
   *  4 MHz < 5 MHz : within spec
   */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, ADXL1_CS_Pin|GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pins : ADXL1_CS_Pin PA1 */
  GPIO_InitStruct.Pin = ADXL1_CS_Pin|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : INT2_Pin */
  GPIO_InitStruct.Pin = INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(INT2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// Overrides putchar function to transmit printing to ST-LINK via USART2
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  printf("[ERROR]");
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
