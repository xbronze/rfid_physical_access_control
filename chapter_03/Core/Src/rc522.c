#include "rc522.h"

extern SPI_HandleTypeDef hspi1;

/* 片选与复位引脚操作 */
#define RC522_CS_LOW()      HAL_GPIO_WritePin(RC522_CS_GPIO_Port,  RC522_CS_Pin,  GPIO_PIN_RESET)
#define RC522_CS_HIGH()     HAL_GPIO_WritePin(RC522_CS_GPIO_Port,  RC522_CS_Pin,  GPIO_PIN_SET)
#define RC522_RST_LOW()     HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_RESET)
#define RC522_RST_HIGH()    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET)

/* ==================== 底层寄存器读写 ==================== */
void RC522_WriteReg(uint8_t addr, uint8_t value)
{
    uint8_t txData[2];
    txData[0] = (addr << 1) & 0x7E;
    txData[1] = value;

    RC522_CS_LOW();
    HAL_SPI_Transmit(&hspi1, txData, 2, 100);
    RC522_CS_HIGH();
}

uint8_t RC522_ReadReg(uint8_t addr)
{
    uint8_t txData[2];
    uint8_t rxData[2];

    txData[0] = ((addr << 1) & 0x7E) | 0x80;
    txData[1] = 0x00;

    RC522_CS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, txData, rxData, 2, 100);
    RC522_CS_HIGH();

    return rxData[1];
}

static void RC522_SetBitMask(uint8_t addr, uint8_t mask)
{
    uint8_t temp = RC522_ReadReg(addr);
    RC522_WriteReg(addr, temp | mask);
}

static void RC522_ClearBitMask(uint8_t addr, uint8_t mask)
{
    uint8_t temp = RC522_ReadReg(addr);
    RC522_WriteReg(addr, temp & (~mask));
}

/* ==================== 复位与天线 ==================== */
void RC522_Reset(void)
{
    RC522_RST_LOW();
    HAL_Delay(10);
    RC522_RST_HIGH();
    HAL_Delay(50);
}

void RC522_AntennaOn(void)
{
    RC522_WriteReg(TxControlReg, 0x83);   /* bit7=Tx2CW 保留，bit0/1=TX1/TX2 RF 输出使能 */
    HAL_Delay(10);
}

/* ==================== 初始化 ==================== */
void RC522_Init(void)
{
    RC522_Reset();

    RC522_WriteReg(CommandReg, PCD_RESETPHASE);
    HAL_Delay(50);

    RC522_WriteReg(TModeReg, 0x8D);
    RC522_WriteReg(TPrescalerReg, 0x3E);
    RC522_WriteReg(TReloadRegL, 30);
    RC522_WriteReg(TReloadRegH, 0);

    RC522_WriteReg(TxASKReg, 0x40);
    RC522_WriteReg(ModeReg, 0x3D);

    RC522_AntennaOn();
}

/* ==================== 公共收发函数 ==================== */
static uint8_t RC522_ToCard(uint8_t command,
                            uint8_t *sendData, uint8_t sendLen,
                            uint8_t *backData, uint16_t *backLen)
{
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    RC522_WriteReg(ComIEnReg, irqEn | 0x80);
    RC522_ClearBitMask(ComIrqReg, 0x80);
    RC522_SetBitMask(FIFOLevelReg, 0x80);
    RC522_WriteReg(CommandReg, PCD_IDLE);

    for (i = 0; i < sendLen; i++) {
        RC522_WriteReg(FIFODataReg, sendData[i]);
    }

    RC522_WriteReg(CommandReg, command);

    if (command == PCD_TRANSCEIVE) {
        RC522_SetBitMask(BitFramingReg, 0x80);
    }

    i = 2000;
    do {
        n = RC522_ReadReg(ComIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    RC522_ClearBitMask(BitFramingReg, 0x80);

    if (i == 0) {
        return MI_NOTAGERR;
    }

    if (RC522_ReadReg(ErrorReg) & 0x1B) {
        return MI_ERR;
    }

    status = MI_OK;
    if (backData && backLen) {
        uint8_t unLen = RC522_ReadReg(FIFOLevelReg);
        lastBits = RC522_ReadReg(ControlReg) & 0x07;
        if (lastBits) {
            *backLen = (unLen - 1) * 8 + lastBits;
        } else {
            *backLen = unLen * 8;
        }
        if (unLen == 0) unLen = 1;
        if (unLen > 16) unLen = 16;
        for (i = 0; i < unLen; i++) {
            backData[i] = RC522_ReadReg(FIFODataReg);
        }
    }

    return status;
}

/* ==================== 寻卡 ==================== */
uint8_t RC522_Request(uint8_t reqMode, uint8_t *TagType)
{
    uint8_t status;
    uint16_t backBits;

    RC522_WriteReg(BitFramingReg, 0x07);
    TagType[0] = reqMode;
    status = RC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

    if ((status != MI_OK) || (backBits != 0x10)) {
        status = MI_ERR;
    }

    return status;
}

/* ==================== 防冲突 ==================== */
uint8_t RC522_Anticoll(uint8_t *serNum)
{
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint16_t unLen;
    uint8_t buffer[5];

    RC522_WriteReg(BitFramingReg, 0x00);

    buffer[0] = 0x93;
    buffer[1] = 0x20;

    status = RC522_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &unLen);

    if (status == MI_OK) {
        if (unLen != 40) {
            status = MI_ERR;
        } else {
            for (i = 0; i < 4; i++) {
                serNum[i] = buffer[i];
                serNumCheck ^= buffer[i];
            }
            if (serNumCheck != buffer[4]) {
                status = MI_ERR;
            }
        }
    }

    return status;
}

/* ==================== 卡片休眠 ==================== */
void RC522_Halt(void)
{
    uint16_t unLen;
    uint8_t buffer[2];

    buffer[0] = 0x50;
    buffer[1] = 0x00;

    RC522_WriteReg(BitFramingReg, 0x00);
    RC522_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &unLen);
}
