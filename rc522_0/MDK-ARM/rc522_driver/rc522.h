/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rc522.h
  * @brief   MFRC522 RFID reader driver - SPI low-level layer
  *          Pin map (from STM32CubeMX):
  *            PA4 -> RC522 SDA/CS (software NSS)
  *            PA3 -> RC522 RST   (hard reset)
  *            PA5 -> SPI1 SCK, PA6 -> SPI1 MISO, PA7 -> SPI1 MOSI
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __RC522_H__
#define __RC522_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"

/* RC522 GPIO mapping */
#define RC522_CS_GPIO_Port   GPIOA
#define RC522_CS_Pin         GPIO_PIN_4
#define RC522_RST_GPIO_Port  GPIOA
#define RC522_RST_Pin        GPIO_PIN_3

#define RC522_CS_LOW()   HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_RESET)
#define RC522_CS_HIGH()  HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_SET)
#define RC522_RST_LOW()  HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_RESET)
#define RC522_RST_HIGH() HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET)

/* MFRC522 register addresses */
typedef enum {
  RC522_REG_COMMAND      = 0x01,
  RC522_REG_COM_IRQ      = 0x04,
  RC522_REG_ERROR        = 0x06,
  RC522_REG_STATUS1      = 0x07,
  RC522_REG_STATUS2      = 0x08,
  RC522_REG_FIFO_DATA    = 0x09,
  RC522_REG_FIFO_LEVEL   = 0x0A,
  RC522_REG_CONTROL      = 0x0C,
  RC522_REG_BIT_FRAMING  = 0x0D,
  RC522_REG_MODE         = 0x11,
  RC522_REG_TX_MODE      = 0x12,
  RC522_REG_RX_MODE      = 0x13,
  RC522_REG_TX_CONTROL   = 0x14,
  RC522_REG_TX_ASK       = 0x15,
  RC522_REG_T_MODE       = 0x2A,
  RC522_REG_T_PRESCALER  = 0x2B,
  RC522_REG_T_RELOAD0    = 0x2C,
  RC522_REG_T_RELOAD1    = 0x2D,
  RC522_REG_VERSION      = 0x37
} RC522_Reg_t;

#define RC522_CMD_SOFT_RESET 0x0F
#define RC522_VERSION_V1_0   0x91
#define RC522_VERSION_V2_0   0x92
#define RC522_VERSION_CLONE  0x88

void    RC522_Init(void);
void    RC522_HardReset(void);
uint8_t RC522_ReadRegister(uint8_t reg);
void    RC522_WriteRegister(uint8_t reg, uint8_t value);
uint8_t RC522_GetVersion(void);
void    RC522_AntennaOff(void);

#ifdef __cplusplus
}
#endif
#endif /* __RC522_H__ */
