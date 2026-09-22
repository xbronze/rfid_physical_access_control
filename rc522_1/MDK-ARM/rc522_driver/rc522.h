#ifndef __RC522_H
#define __RC522_H

#include "main.h"

/* ==================== Pin Definitions ==================== */
/* Modify these to match your CubeMX GPIO configuration */
#define RC522_CS_PORT       GPIOA
#define RC522_CS_PIN        GPIO_PIN_4

#define RC522_RST_PORT      GPIOA
#define RC522_RST_PIN       GPIO_PIN_3

/* ==================== RC522 Register Addresses ==================== */
#define RC522_REG_COMMAND       0x01
#define RC522_REG_COMIEN        0x02
#define RC522_REG_DIVIEN        0x03
#define RC522_REG_COMIRQ        0x04
#define RC522_REG_DIVIRQ        0x05
#define RC522_REG_ERROR         0x06
#define RC522_REG_STATUS1       0x07
#define RC522_REG_STATUS2       0x08
#define RC522_REG_FIFODATA      0x09
#define RC522_REG_FIFOLEVEL     0x0A
#define RC522_REG_WATERLEVEL    0x0B
#define RC522_REG_CONTROL       0x0C
#define RC522_REG_BITFRAMING    0x0D
#define RC522_REG_COLL          0x0E
#define RC522_REG_MODE          0x11
#define RC522_REG_TXCONTROL     0x14
#define RC522_REG_TXASK         0x15
#define RC522_REG_TXSEL         0x16
#define RC522_REG_RXSEL         0x17
#define RC522_REG_RXTHRESHOLD   0x18
#define RC522_REG_DEMOD         0x19
#define RC522_REG_RFCFG         0x26
#define RC522_REG_TMOD          0x2A
#define RC522_REG_TPRESCALER    0x2B
#define RC522_REG_TReloadRegH   0x2C
#define RC522_REG_TReloadRegL   0x2D
#define RC522_REG_VERSION       0x37
#define RC522_REG_CRCRESULT_M   0x21
#define RC522_REG_CRCRESULT_L   0x22

/* ==================== PCD Commands ==================== */
#define PCD_IDLE                0x00
#define PCD_CALCCRC             0x03
#define PCD_TRANSCEIVE          0x0C
#define PCD_RESETPHASE          0x0F

/* ==================== PICC Commands ==================== */
#define PICC_REQIDL             0x26    /* REQ-A: query cards in IDLE state */
#define PICC_REQALL             0x52    /* WUP-A: query cards in IDLE/READY state */
#define PICC_ANTICOLL           0x93    /* Anti-collision level 1 */
#define PICC_HALT               0x50    /* Halt card */

/* ==================== Function Declarations ==================== */
void    RC522_Init(void);
uint8_t RC522_ReadUID(uint8_t *uid);
/* Low-level functions; may be useful during debugging */
void    RC522_WriteReg(uint8_t addr, uint8_t value);
uint8_t RC522_ReadReg(uint8_t addr);
void    RC522_SetBitMask(uint8_t addr, uint8_t mask);
void    RC522_ClearBitMask(uint8_t addr, uint8_t mask);
void    RC522_AntennaOn(void);
void    RC522_AntennaOff(void);
void    RC522_Reset(void);
uint8_t RC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen,
                     uint8_t *recvData, uint16_t *recvLen);
uint8_t RC522_Request(uint8_t reqMode, uint8_t *tagType);
uint8_t RC522_Anticoll(uint8_t *uid);
void    RC522_CalcCRC(uint8_t *data, uint8_t len, uint8_t *crcOut);
/* Single-shot WUPA probe; always drains FIFO. rx[8] out, returns status */
uint8_t RC522_DebugRequest(uint8_t *rx, uint8_t *err, uint8_t *lastBits, uint8_t *level);

#endif /* __RC522_H */
