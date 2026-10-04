#ifndef __RC522_H
#define __RC522_H

#include "main.h"
#include "spi.h"
#include <stdio.h>
#include <string.h>

/* 引脚后备定义：CS=PA4，RST=PA3 */
#ifndef RC522_CS_GPIO_Port
#define RC522_CS_GPIO_Port      GPIOA
#define RC522_CS_Pin            GPIO_PIN_4
#endif

#ifndef RC522_RST_GPIO_Port
#define RC522_RST_GPIO_Port     GPIOA
#define RC522_RST_Pin           GPIO_PIN_3
#endif

/* 状态返回值 */
#define MI_OK           0
#define MI_NOTAGERR     1
#define MI_ERR          2

/* 寻卡模式 */
#define PICC_REQIDL     0x26    /* REQA：只唤醒 IDLE 状态卡 */
#define PICC_REQALL     0x52    /* WUPA：唤醒 IDLE 和 HALT 状态卡 */

/* RC522寄存器地址 */
#define CommandReg      0x01
#define ComIEnReg       0x02
#define ComIrqReg       0x04
#define DivIrqReg       0x05
#define ErrorReg        0x06
#define Status2Reg      0x08
#define FIFODataReg     0x09
#define FIFOLevelReg    0x0A
#define ControlReg      0x0C
#define BitFramingReg   0x0D
#define CollReg         0x0E
#define ModeReg         0x11
#define TxControlReg    0x14
#define TxASKReg        0x15
#define TModeReg        0x2A
#define TPrescalerReg   0x2B
#define TReloadRegH     0x2C
#define TReloadRegL     0x2D
#define VersionReg      0x37

/* RC522命令字 */
#define PCD_IDLE        0x00
#define PCD_TRANSCEIVE  0x0C
#define PCD_RESETPHASE  0x0F

/* 函数声明 */
void    RC522_Init(void);
uint8_t RC522_ReadReg(uint8_t addr);
void    RC522_WriteReg(uint8_t addr, uint8_t value);
void    RC522_Reset(void);
void    RC522_AntennaOn(void);

uint8_t RC522_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t RC522_Anticoll(uint8_t *serNum);
void    RC522_Halt(void);

#endif /* __RC522_H */
