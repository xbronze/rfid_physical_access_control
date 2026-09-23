/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rc522.c
  * @brief   MFRC522 RFID reader driver - SPI implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "rc522.h"

#define RC522_SPI_READ  0x80
#define RC522_SPI_WRITE 0x00

static uint8_t RC522_SPI_Transfer(uint8_t tx)
{
  uint8_t rx = 0;
  HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, 100);
  return rx;
}

uint8_t RC522_ReadRegister(uint8_t reg)
{
  uint8_t addr = (uint8_t)(((reg << 1) & 0x7E) | RC522_SPI_READ);
  RC522_CS_LOW();
  RC522_SPI_Transfer(addr);
  uint8_t value = RC522_SPI_Transfer(0x00);
  RC522_CS_HIGH();
  return value;
}

void RC522_WriteRegister(uint8_t reg, uint8_t value)
{
  uint8_t addr = (uint8_t)((reg << 1) & 0x7E);
  RC522_CS_LOW();
  RC522_SPI_Transfer(addr);
  RC522_SPI_Transfer(value);
  RC522_CS_HIGH();
}

void RC522_HardReset(void)
{
  RC522_RST_HIGH();
  HAL_Delay(5);
  RC522_RST_LOW();
  HAL_Delay(5);
  RC522_RST_HIGH();
  HAL_Delay(50);
}

void RC522_AntennaOff(void)
{
  uint8_t tx = RC522_ReadRegister(RC522_REG_TX_CONTROL);
  tx &= (uint8_t)~0x03;
  RC522_WriteRegister(RC522_REG_TX_CONTROL, tx);
}

void RC522_Init(void)
{
  RC522_CS_HIGH();
  RC522_HardReset();
  RC522_AntennaOff();
}

uint8_t RC522_GetVersion(void)
{
  return RC522_ReadRegister(RC522_REG_VERSION);
}
