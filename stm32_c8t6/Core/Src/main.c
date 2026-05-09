/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : F103 Gateway RX Node (Production Ready)
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint8_t rx_buffer[64]; // [??] ????????? (Receive Buffer)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);

/* USER CODE BEGIN PFP */
uint8_t SX1278_ReadReg(uint8_t addr);
void SX1278_WriteReg(uint8_t addr, uint8_t data);

// ???????
void SX1278_LoRa_Init(void);
void SX1278_ReadFIFO(uint8_t *buf, uint8_t len);
void Parse_Data(char *buf, int rssi);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include <stdio.h>
#include <string.h>
/* USER CODE END 0 */

int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();

  /* USER CODE BEGIN 2 */

  // 1. ????
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  HAL_Delay(50);

  // 2. ???????
  uint8_t version = SX1278_ReadReg(0x42);
  if (version != 0x12) {
    uint8_t err_msg[] = "[FATAL] SX1278 Not Found! System Halted.\r\n";
    HAL_UART_Transmit(&huart1, err_msg, sizeof(err_msg) - 1, 100);
    while (1)
      ;
  }

  // 3. ???????? LoRa ???
  SX1278_LoRa_Init();

  uint8_t boot_msg[] = "\r\n[Gateway] LoRa Core Init OK.\r\n[Gateway] "
                       "Listening on 433MHz, SF7, BW125...\r\n";
  HAL_UART_Transmit(&huart1, boot_msg, sizeof(boot_msg) - 1, 100);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // ========================================================
    // ?????? (Complete RX Flow)
    // ========================================================
    uint8_t irq_flags = SX1278_ReadReg(0x12);

    if (irq_flags & 0x40) // ?????? RxDone (Bit 6)
    {
      // [??????] ?????????? FIFO ????????
      uint8_t rx_start_addr = SX1278_ReadReg(0x10); // FifoRxCurrentAddr

      // ????????
      uint8_t len = SX1278_ReadReg(0x13); // RxNbBytes

      // ????,???????
      if (len > 63)
        len = 63;

      // ? SPI ?????????????
      SX1278_WriteReg(0x0D, rx_start_addr); // FifoAddrPtr = FifoRxCurrentAddr

      // ?? FIFO ??? rx_buffer
      SX1278_ReadFIFO(rx_buffer, len);
      rx_buffer[len] = '\0'; // ?????,??????

      // ?????? (RSSI)
      // HF ?? (? 433MHz) ?????: ???? - 157
      int rssi = SX1278_ReadReg(0x1A) - 157;

      // ??????????????
      Parse_Data((char *)rx_buffer, rssi);

      // ??????? (? 1 ??????)
      SX1278_WriteReg(0x12, 0xFF);
    }

    // ????
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

    // ?????? (50ms)
    HAL_Delay(50);
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */

// ======================================================================
// ?????:?????? (Data Parsing Engine / Tr�nh ph�n t�ch d? li?u)
// ======================================================================
void Parse_Data(char *buf, int rssi) {
  int node_id;
  float temp, humi;
  char report[128];

  // ?? sscanf ?????? "NodeID,Temp,Humi" ????
  // ??:????????,???? Keil ??? Target ??????? "Use MicroLIB"
  if (sscanf(buf, "%d,%f,%f", &node_id, &temp, &humi) == 3) {
    // ????,???????
    int len = sprintf(report,
                      "\r\n[Data Parsed] Node: %02d | Temp: %.1f C | Humi: "
                      "%.1f %% | RSSI: %d dBm\r\n",
                      node_id, temp, humi, rssi);
    HAL_UART_Transmit(&huart1, (uint8_t *)report, len, 100);
  } else {
    // ????,??????????
    int len =
        sprintf(report, "\r\n[Raw Frame] %s (RSSI: %d dBm)\r\n", buf, rssi);
    HAL_UART_Transmit(&huart1, (uint8_t *)report, len, 100);
  }
}

// ======================================================================
// ?????:LoRa ???? (LoRa Driver Module / M�-dun tr�nh di?u khi?n LoRa)
// ======================================================================

/**
 * @brief  ?? SX1278 FIFO ?????
 */
void SX1278_ReadFIFO(uint8_t *buf, uint8_t len) {
  // ? SX1278 ?,?? 0x00 ???????????????
  // ???????????? 0x00 ?????????
  for (int i = 0; i < len; i++) {
    buf[i] = SX1278_ReadReg(0x00);
  }
}

/**
 * @brief  ?????? LoRa RX ???
 */
void SX1278_LoRa_Init(void) {
  // 1. Sleep
  SX1278_WriteReg(0x01, 0x80);
  HAL_Delay(10);

  // 2. ??? LoRa Standby ??
  SX1278_WriteReg(0x01, 0x81);
  HAL_Delay(10);

  // 3. ??(433MHz = 0x6C4000)
  SX1278_WriteReg(0x06, 0x6C);
  SX1278_WriteReg(0x07, 0x40);
  SX1278_WriteReg(0x08, 0x00);

  // 4. LNA ????,???????
  SX1278_WriteReg(0x0C, 0x23);

  // 5. ???????: BW125 (0x72), SF7 (0x74)
  SX1278_WriteReg(0x1D, 0x72);
  SX1278_WriteReg(0x1E, 0x74);

  // 6. ?? FIFO ?????
  SX1278_WriteReg(0x0E, 0x00); // TxBase
  SX1278_WriteReg(0x0F, 0x00); // RxBase

  // 7. ???
  SX1278_WriteReg(0x12, 0xFF);

  // 8. ??? RxContinuous ??????
  SX1278_WriteReg(0x01, 0x85);
}

// ---------------------------------------------
// ?? SPI ???? (SPI Primitives)
// ---------------------------------------------
uint8_t SX1278_ReadReg(uint8_t addr) {
  uint8_t tx[2];
  uint8_t rx[2];

  tx[0] = addr & 0x7F;
  tx[1] = 0x00;

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
  // [??????] ???? SPI ????
  HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 100);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  if (status != HAL_OK) {
    return 0xEE; // ???????
  }

  return rx[1];
}

void SX1278_WriteReg(uint8_t addr, uint8_t data) {
  uint8_t tx[2];
  tx[0] = addr | 0x80;
  tx[1] = data;
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, tx, 2, 100);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

/* USER CODE END 4 */

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_SPI1_Init(void) {
  __HAL_RCC_SPI1_CLK_ENABLE();
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_USART1_UART_Init(void) {
  __HAL_RCC_USART1_CLK_ENABLE();
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {
  }
}