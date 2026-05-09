/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : L431 Transmitter Node (TX) Main program body (SPI1 Version)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
volatile uint8_t version = 0;
volatile uint32_t heartbeat = 0;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
uint8_t SX1278_ReadReg(uint8_t addr);
void SX1278_WriteReg(uint8_t addr, uint8_t data);
void SX1278_Init_TX(void);
void SX1278_Transmit(uint8_t *data, uint8_t len);

// --- Debug Variables ---
volatile HAL_StatusTypeDef dbg_spi_status;
volatile uint8_t dbg_spi_rx[2];
// -----------------------
/* USER CODE END PFP */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  
  MX_GPIO_Init();
  MX_SPI1_Init();
  
  // ========================================================
  // SPI1 Pin Mapping (PA4, PA5, PA6, PA7)
  // ========================================================
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // 1. NSS (PA4)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // 2. RST (PB0)
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  
  // 3. SCK, MISO, MOSI (PA5, PA6, PA7) -> AF5_SPI1
  GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL; 
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1; 
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  // ========================================================

  /* USER CODE BEGIN 2 */
  // 1. Hardware Reset SX1278
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); 
  HAL_Delay(20); 
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);   
  HAL_Delay(50); 

  // 2. Read Silicon Version
  version = SX1278_ReadReg(0x42);
  
  if(version != 0x12) {
      // ?? ????????? (The breakpoint MUST be here)
      while(1); 
  }

  // 3. Init TX Mode
  SX1278_Init_TX();
  /* USER CODE END 2 */

  /* Infinite loop */
  while (1)
  {
    /* USER CODE BEGIN 3 */
    uint32_t temp_int = 25;                       
    uint32_t temp_dec = heartbeat % 10;           
    uint32_t humi_int = 60;                       
    uint32_t humi_dec = (heartbeat * 3) % 10;     

    uint8_t payload[50];
    int len = sprintf((char*)payload, "1,%u.%u,%u.%u", temp_int, temp_dec, humi_int, humi_dec);

    SX1278_Transmit(payload, len);

    heartbeat++;
    HAL_Delay(1000); 
    /* USER CODE END 3 */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) { Error_Handler(); }

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) { Error_Handler(); }
}

/* USER CODE BEGIN 4 */
// --- Bare-metal SPI Transfer to bypass HAL bugs ---
uint8_t SX1278_SPI_Transfer(uint8_t tx_data)
{
    // Ensure SPI is enabled
    if ((hspi1.Instance->CR1 & SPI_CR1_SPE) == 0) {
        hspi1.Instance->CR1 |= SPI_CR1_SPE;
    }
    
    // Wait for TX FIFO to have space
    while ((hspi1.Instance->SR & SPI_SR_TXE) == 0) {}
    
    // Write 8-bit data (STM32L4 requires 8-bit pointer cast for 8-bit mode)
    *(__IO uint8_t *)&hspi1.Instance->DR = tx_data;
    
    // Wait for RX FIFO to receive data
    while ((hspi1.Instance->SR & SPI_SR_RXNE) == 0) {}
    
    // Read 8-bit data
    return *(__IO uint8_t *)&hspi1.Instance->DR;
}

uint8_t SX1278_ReadReg(uint8_t addr)
{
    uint8_t rx_data;
    
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    SX1278_SPI_Transfer(addr & 0x7F);        // Send Read Address
    rx_data = SX1278_SPI_Transfer(0xFF);     // Send Dummy Byte 0xFF (Changed from 0x00 for loopback testing)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    
    // Save to debug variables for watch window
    dbg_spi_status = HAL_OK;
    dbg_spi_rx[0] = addr;
    dbg_spi_rx[1] = rx_data;
    
    return rx_data;
}

void SX1278_WriteReg(uint8_t addr, uint8_t data)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); 
    SX1278_SPI_Transfer(addr | 0x80);        // Send Write Address
    SX1278_SPI_Transfer(data);               // Send Data
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);   
}

void SX1278_Init_TX(void)
{
    SX1278_WriteReg(0x01, 0x80); 
    HAL_Delay(10);
    SX1278_WriteReg(0x01, 0x81); 
    HAL_Delay(10);
    SX1278_WriteReg(0x06, 0x6C);
    SX1278_WriteReg(0x07, 0x40);
    SX1278_WriteReg(0x08, 0x00);
    SX1278_WriteReg(0x09, 0xFF); 
    SX1278_WriteReg(0x1D, 0x72);
    SX1278_WriteReg(0x1E, 0x74);
    SX1278_WriteReg(0x0E, 0x00); 
    SX1278_WriteReg(0x0F, 0x00); 
}

void SX1278_Transmit(uint8_t *data, uint8_t len)
{
    SX1278_WriteReg(0x01, 0x81);
    SX1278_WriteReg(0x0D, 0x00);
    SX1278_WriteReg(0x22, len);
    for(uint8_t i = 0; i < len; i++) {
        SX1278_WriteReg(0x00, data[i]);
    }
    SX1278_WriteReg(0x01, 0x8B);
    while((SX1278_ReadReg(0x12) & 0x08) == 0) { }
    SX1278_WriteReg(0x12, 0x08);
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}