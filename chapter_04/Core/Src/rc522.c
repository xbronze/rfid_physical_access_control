#include "rc522.h"

extern SPI_HandleTypeDef hspi1;

#define RC522_CS_LOW()      HAL_GPIO_WritePin(RC522_CS_GPIO_Port,  RC522_CS_Pin,  GPIO_PIN_RESET)
#define RC522_CS_HIGH()     HAL_GPIO_WritePin(RC522_CS_GPIO_Port,  RC522_CS_Pin,  GPIO_PIN_SET)
#define RC522_RST_LOW()     HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_RESET)
#define RC522_RST_HIGH()    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET)

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

void RC522_Reset(void)
{
    RC522_RST_LOW();
    HAL_Delay(10);
    RC522_RST_HIGH();
    HAL_Delay(50);
}

void RC522_AntennaOn(void)
{
    uint8_t temp = RC522_ReadReg(TxControlReg);
    if (!(temp & 0x03)) {
        RC522_SetBitMask(TxControlReg, 0x03);
    }
}

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
    
    // 初始开启 CRC 预设值 0x6363
    RC522_WriteReg(ModeReg, 0x3D); 
    RC522_AntennaOn();
}

static uint8_t RC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen)
{
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    if (command == PCD_AUTHENT) {
        RC522_WriteReg(TxModeReg, 0x80); // 认证开启发送 CRC
        RC522_WriteReg(RxModeReg, 0x80); // 认证开启接收 CRC
        irqEn = 0x77;
        waitIRq = 0x30;
    } else if (command == PCD_TRANSCEIVE) {
        // 寻卡(0x52/0x26)和防冲突(0x93 0x20)不需要 CRC
        if ((sendData[0] == 0x93 && sendData[1] == 0x20) || sendData[0] == 0x52 || sendData[0] == 0x26) {
            RC522_WriteReg(TxModeReg, 0x00);
            RC522_WriteReg(RxModeReg, 0x00);
        } else {
            // 选卡(0x93 0x70)和读写(0x30/0xA0)需要 CRC
            RC522_WriteReg(TxModeReg, 0x80);
            RC522_WriteReg(RxModeReg, 0x80);
        }
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    // 【新增补丁】如果是写块指令（0xA0），强制等待发送完成（IdleIRq 0x10）
    // 防止卡片回复 ACK 导致提前打断后续 16 字节数据的发送
    if (command == PCD_TRANSCEIVE && sendLen == 18 && sendData[0] == 0xA0) {
        waitIRq = 0x10; 
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

    // 循环放大到 20000 次，防止 SPI 降速后误判超时
    i = 20000; 
    do {
        n = RC522_ReadReg(ComIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    RC522_ClearBitMask(BitFramingReg, 0x80);

    if (i == 0) {
        return MI_NOTAGERR; // 彻底超时
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

uint8_t RC522_Select(uint8_t *serNum)
{
    uint8_t status;
    uint16_t unLen;
    uint8_t buffer[7];
    uint8_t i;

    buffer[0] = 0x93;       /* Select 指令 */
    buffer[1] = 0x70;       /* 选卡 */
    for (i = 0; i < 4; i++) {
        buffer[2 + i] = serNum[i];
    }
    /* 计算 BCC（UID 4字节异或） */
    buffer[6] = serNum[0] ^ serNum[1] ^ serNum[2] ^ serNum[3];

    RC522_WriteReg(BitFramingReg, 0x00);
    status = RC522_ToCard(PCD_TRANSCEIVE, buffer, 7, buffer, &unLen);
    
    // 只要底层没报错返回 MI_OK，就说明选卡成功
    return status;
}

void RC522_Halt(void)
{
    uint16_t unLen;
    uint8_t buffer[2];
    buffer[0] = 0x50;
    buffer[1] = 0x00;
    RC522_WriteReg(BitFramingReg, 0x00);
    RC522_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &unLen);
}

uint8_t MIFARE_Auth(uint8_t blockAddr, uint8_t authMode, uint8_t *key, uint8_t *serNum)
{
    uint8_t status;
    uint16_t backBits;
    uint8_t buffer[12];
    uint8_t i;

    buffer[0] = authMode;
    buffer[1] = blockAddr;
    for (i = 0; i < 6; i++) {
        buffer[2 + i] = key[i];
    }
    for (i = 0; i < 4; i++) {
        buffer[8 + i] = serNum[i];
    }

    status = RC522_ToCard(PCD_AUTHENT, buffer, 12, NULL, &backBits);
    
    if (status == MI_OK) {
        if (!(RC522_ReadReg(Status2Reg) & 0x08)) {
            status = MI_ERR;
        }
    }
    return status;
}

uint8_t MIFARE_ReadBlock(uint8_t blockAddr, uint8_t *data)
{
    uint8_t status;
    uint16_t unLen;
    uint8_t buffer[2];
    buffer[0] = 0x30;
    buffer[1] = blockAddr;
    status = RC522_ToCard(PCD_TRANSCEIVE, buffer, 2, data, &unLen);
    return status;
}

uint8_t MIFARE_WriteBlock(uint8_t blockAddr, uint8_t *data)
{
    uint8_t status;
    uint16_t unLen;
    uint8_t buffer[2];

    // 1. 发送写块指令和地址 (0xA0 + Addr)
    buffer[0] = 0xA0;
    buffer[1] = blockAddr;
    RC522_WriteReg(BitFramingReg, 0x00);
    status = RC522_ToCard(PCD_TRANSCEIVE, buffer, 2, NULL, &unLen);
    if (status != MI_OK) return status;

    // 2. 发送 16 字节数据
    status = RC522_ToCard(PCD_TRANSCEIVE, data, 16, NULL, &unLen);
    return status;
}
