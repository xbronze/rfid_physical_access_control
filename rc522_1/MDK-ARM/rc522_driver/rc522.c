#include "rc522.h"
#include "spi.h"        /* SPI handle from CubeMX-generated spi.c */

extern SPI_HandleTypeDef hspi1;   /* SPI instance used by this driver */

/* ==================== Low-level SPI Read/Write ==================== */

/**
 * @brief  Write one byte to an RC522 register
 * @param  addr  Register address (0x00 ~ 0x3F)
 * @param  value Data to write
 */
void RC522_WriteReg(uint8_t addr, uint8_t value)
{
    uint8_t txBuf[2];
    /* RC522 write frame: address shifted left 1 bit, LSB = 0 */
    txBuf[0] = (addr << 1) & 0x7E;
    txBuf[1] = value;

    HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, txBuf, 2, 100);
    HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_SET);
}

/**
 * @brief  Read one byte from an RC522 register
 * @param  addr  Register address
 * @retval Read data byte
 */
uint8_t RC522_ReadReg(uint8_t addr)
{
    uint8_t txBuf[2], rxBuf[2];
    /* RC522 read frame: address shifted left 1 bit, LSB = 1 */
    txBuf[0] = ((addr << 1) & 0x7E) | 0x80;
    txBuf[1] = 0x00;

    HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi1, txBuf, rxBuf, 2, 100);
    HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_SET);

    return rxBuf[1];
}

/**
 * @brief  Set specific bits in a register
 */
void RC522_SetBitMask(uint8_t addr, uint8_t mask)
{
    uint8_t temp = RC522_ReadReg(addr);
    RC522_WriteReg(addr, temp | mask);
}

/**
 * @brief  Clear specific bits in a register
 */
void RC522_ClearBitMask(uint8_t addr, uint8_t mask)
{
    uint8_t temp = RC522_ReadReg(addr);
    RC522_WriteReg(addr, temp & (~mask));
}

/* ==================== Antenna Control ==================== */

/**
 * @brief  Turn on antenna (TX1 + TX2)
 */
void RC522_AntennaOn(void)
{
    RC522_SetBitMask(RC522_REG_TXCONTROL, 0x03);
}

/**
 * @brief  Turn off antenna
 */
void RC522_AntennaOff(void)
{
    RC522_ClearBitMask(RC522_REG_TXCONTROL, 0x03);
}

/* ==================== Reset ==================== */

/**
 * @brief  Hardware reset RC522 via RST pin
 */
void RC522_Reset(void)
{
    HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(50);
}

/* ==================== Initialization ==================== */

/**
 * @brief  Initialize RC522
 * @note   Call after SPI peripheral is initialized
 */
void RC522_Init(void)
{
    /* 1. Hardware reset */
    RC522_Reset();

    /* 2. Soft reset */
    RC522_WriteReg(RC522_REG_COMMAND, PCD_RESETPHASE);
    HAL_Delay(10);

    /* Safety-critical registers */
    RC522_WriteReg(0x10, 0x00);          /* TxModeReg: 106 kbps */
    RC522_WriteReg(0x12, 0x00);          /* RxModeReg: 106 kbps */
    RC522_WriteReg(0x24, 0x26);          /* ModWidthReg: modulation width */

    /* 3. Timer configuration */
    RC522_WriteReg(RC522_REG_TMOD, 0x8D);           /* Timer: auto restart, clk = fOSC/16 */
    RC522_WriteReg(RC522_REG_TPRESCALER, 0x3E);     /* Prescaler = 62 */
    RC522_WriteReg(RC522_REG_TReloadRegL, 30);      /* Reload value low byte */
    RC522_WriteReg(RC522_REG_TReloadRegH, 0);       /* Reload value high byte */

    /* 4. TX/RX mode: 106 kbps, NRZ encoding */
    RC522_WriteReg(RC522_REG_TXASK, 0x40);
    RC522_WriteReg(RC522_REG_MODE, 0x3D);           /* CRC preset 0x6363 */
    RC522_WriteReg(RC522_REG_RFCFG, 0x40);          /* RxGain 33dB (safe default) */

    /* 5. Turn on antenna */
    RC522_AntennaOn();
}

/* ==================== Card Communication ==================== */

/**
 * @brief  Transceive: send command to card and receive response
 * @param  command   PCD command (e.g. PCD_TRANSCEIVE)
 * @param  sendData  Data to send
 * @param  sendLen   Number of bytes to send
 * @param  recvData  Receive buffer
 * @param  recvLen   Input: max buffer size; Output: actual received byte count
 * @retval 0 = success, non-zero = failure
 */
uint8_t RC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen,
                     uint8_t *recvData, uint16_t *recvLen)
{
    uint8_t status = 1;     /* default: failure */
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint16_t i;

    if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;       /* Enable RxIrq, TxIrq, IdleIrq */
        waitIRq = 0x30;     /* Wait for RxIrq or IdleIrq */
    }

    /* enable & clear IRQs */
    RC522_WriteReg(RC522_REG_COMIEN, irqEn | 0x80);
    /* ComIrq bits are write-1-to-clear: write 0x7F to clear ALL pending
       flags. Writing 0x80 (reserved bit) leaves stale RxIrq/IdleIrq set,
       which makes the next transceive "succeed" immediately on garbage. */
    RC522_WriteReg(RC522_REG_COMIRQ, 0x7F);
    RC522_SetBitMask(RC522_REG_FIFOLEVEL, 0x80);    /* Flush FIFO */

    /* Set idle, write data to FIFO */
    RC522_WriteReg(RC522_REG_COMMAND, PCD_IDLE);
    for (i = 0; i < sendLen; i++) {
        RC522_WriteReg(RC522_REG_FIFODATA, sendData[i]);
    }
    /* Execute command */
    RC522_WriteReg(RC522_REG_COMMAND, command);
    if (command == PCD_TRANSCEIVE) {
        RC522_SetBitMask(RC522_REG_BITFRAMING, 0x80); /* StartSend */
    }

    /* Wait for completion */
    i = 2000;
    do {
        uint8_t irq = RC522_ReadReg(RC522_REG_COMIRQ);
        if (irq & waitIRq) {
            status = 0;
            break;
        }
        if (irq & 0x01) {   /* TimerIrq */
            status = 1;
            break;
        }
    } while (--i);

    RC522_ClearBitMask(RC522_REG_BITFRAMING, 0x80);

    if (status == 0) {
        uint8_t error = RC522_ReadReg(RC522_REG_ERROR);
        if (error & 0x1B) {     /* Error bits set */
            status = 1;
        }
    }

    if (status == 0 && recvData != NULL) {
        uint8_t fifoLevel = RC522_ReadReg(RC522_REG_FIFOLEVEL);
        uint8_t lastBits = RC522_ReadReg(RC522_REG_CONTROL) & 0x07;
        if (lastBits) {
            *recvLen = (fifoLevel - 1) * 8 + lastBits;
        } else {
            *recvLen = fifoLevel * 8;
        }
        if (fifoLevel == 0) {
            fifoLevel = 1;
        }
        if (fifoLevel > *recvLen / 8) {
            fifoLevel = *recvLen / 8;
        }
        for (i = 0; i < fifoLevel; i++) {
            recvData[i] = RC522_ReadReg(RC522_REG_FIFODATA);
        }
        /* NOTE: on return *recvLen is in BYTES (see conversion below).
           Callers must compare 2 for ATQA and 5 for UID+BCC, not 16/40. */
        *recvLen = (*recvLen + 7) / 8;   /* Convert to byte count */
    }

    return status;
}

/* ==================== Request (寻卡) ==================== */

/**
 * @brief  Request card type (REQA/WUPA)
 * @param  reqMode  Request mode (PICC_REQIDL or PICC_REQALL)
 * @param  tagType  Output: card type (2 bytes ATQA)
 * @retval 0 = success, non-zero = failure
 */
uint8_t RC522_Request(uint8_t reqMode, uint8_t *tagType)
{
    uint8_t status;
    uint16_t backBits = 0;
    uint8_t retry;
    uint8_t txCmd;              /* dedicated TX buffer, must NOT reuse tagType */

    tagType[0] = 0;
    tagType[1] = 0;
    txCmd = reqMode;

    for (retry = 0; retry < 5; retry++) {
        RC522_WriteReg(RC522_REG_BITFRAMING, 0x07);   /* 7-bit short frame */
        status = RC522_ToCard(PCD_TRANSCEIVE, &txCmd, 1, tagType, &backBits);
        if (status == 0 && backBits == 2) {   /* ToCard returns BYTES: 2-byte ATQA */
            return 0;
        }
        HAL_Delay(10);
    }
    return 1;
}

/*
 * Single-shot WUPA (0x52) diagnostic probe, self-contained (does NOT use
 * RC522_ToCard) so that error/lastBits/FIFOLevel are captured immediately
 * after the transceive and the raw FIFO is ALWAYS drained.
 *
 * Success requires ALL of: RxIrq observed, no protocol/parity/collision/
 * overflow error, and exactly 2 full bytes (16-bit ATQA). This rejects
 * noise-triggered 1-bit phantom frames at high gain.
 */
uint8_t RC522_DebugRequest(uint8_t *rx, uint8_t *err, uint8_t *lastBits, uint8_t *level)
{
    uint8_t cmd = PICC_REQALL;    /* WUPA: answered in IDLE and READY */
    uint16_t n;
    uint8_t irq;
    uint8_t i, got;

    for (i = 0; i < 8; i++) {
        rx[i] = 0;
    }

    RC522_WriteReg(RC522_REG_COMIEN, 0x77 | 0x80);   /* Rx/Tx/Idle/Timer irq */
    RC522_WriteReg(RC522_REG_COMIRQ, 0x7F);          /* clear all (w1c) */
    RC522_SetBitMask(RC522_REG_FIFOLEVEL, 0x80);     /* flush FIFO */
    RC522_WriteReg(RC522_REG_COMMAND, PCD_IDLE);
    RC522_WriteReg(RC522_REG_BITFRAMING, 0x07);      /* 7-bit short frame */
    RC522_WriteReg(RC522_REG_FIFODATA, cmd);
    RC522_WriteReg(RC522_REG_COMMAND, PCD_TRANSCEIVE);
    RC522_SetBitMask(RC522_REG_BITFRAMING, 0x80);   /* StartSend */

    n = 2000;
    do {
        irq = RC522_ReadReg(RC522_REG_COMIRQ);
    } while (!(irq & 0x30) && !(irq & 0x01) && --n);  /* Rx/Idle or Timer */

    RC522_ClearBitMask(RC522_REG_BITFRAMING, 0x80);

    *err      = RC522_ReadReg(RC522_REG_ERROR);
    *lastBits = RC522_ReadReg(RC522_REG_CONTROL) & 0x07;
    *level    = RC522_ReadReg(RC522_REG_FIFOLEVEL) & 0x3F;

    got = (*level > 8) ? 8 : *level;
    for (i = 0; i < got; i++) {
        rx[i] = RC522_ReadReg(RC522_REG_FIFODATA);
    }

    if ((irq & 0x20) && (*err & 0x1B) == 0 &&
        *level == 2 && *lastBits == 0) {
        return 0;   /* valid 16-bit ATQA */
    }
    return 1;
}

/* ==================== Anti-collision ==================== */

/**
 * @brief  Anti-collision: read card UID
 * @param  uid  Output buffer (5 bytes: 4-byte UID + 1-byte BCC)
 * @retval 0 = success, 1 = failure, 2 = BCC check failed
 */
uint8_t RC522_Anticoll(uint8_t *uid)
{
    uint8_t status;
    uint8_t sendData[2];
    uint16_t recvBits = 0;

    RC522_WriteReg(RC522_REG_BITFRAMING, 0x00);     /* 8-bit frame */
    sendData[0] = PICC_ANTICOLL;
    sendData[1] = 0x20;                             /* Anti-collision NVB */

    status = RC522_ToCard(PCD_TRANSCEIVE, sendData, 2, uid, &recvBits);

    if (status == 0 && recvBits == 5) {            /* 5 bytes = 40 bits */
        /* Verify BCC checksum */
        uint8_t check = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];
        if (check != uid[4]) {
            return 2;   /* BCC check failed */
        }
        return 0;
    }
    return 1;
}

/* ==================== Read UID (main interface) ==================== */

/* Calculate ISO14443 CRC-A using the RC522 hardware CRC unit. */
void RC522_CalcCRC(uint8_t *data, uint8_t len, uint8_t *crcOut)
{
    uint8_t i;
    uint16_t wait = 1000;

    RC522_WriteReg(RC522_REG_COMMAND, PCD_IDLE);
    RC522_WriteReg(RC522_REG_DIVIRQ, 0x04);         /* clear CRCIRq (w1c) */
    RC522_SetBitMask(RC522_REG_FIFOLEVEL, 0x80);    /* flush FIFO */
    for (i = 0; i < len; i++) {
        RC522_WriteReg(RC522_REG_FIFODATA, data[i]);
    }
    RC522_WriteReg(RC522_REG_COMMAND, PCD_CALCCRC);
    do {
    } while (!(RC522_ReadReg(RC522_REG_DIVIRQ) & 0x04) && --wait);

    crcOut[0] = RC522_ReadReg(RC522_REG_CRCRESULT_L);   /* LSB @0x22 */
    crcOut[1] = RC522_ReadReg(RC522_REG_CRCRESULT_M);   /* MSB @0x21 */
    RC522_WriteReg(RC522_REG_COMMAND, PCD_IDLE);
}

uint8_t RC522_ReadUID(uint8_t *uid)
{
    uint8_t tagType[2];
    uint8_t uidBuf[5];

    /* 1. Request (WUPA) */
    if (RC522_Request(PICC_REQALL, tagType) != 0) {
        return 1;   /* Request failed */
    }

    /* 2. Anti-collision to get UID */
    if (RC522_Anticoll(uidBuf) != 0) {
        return 2;   /* Anti-collision failed */
    }

    /* 3. Copy 4-byte UID to output */
    uid[0] = uidBuf[0];
    uid[1] = uidBuf[1];
    uid[2] = uidBuf[2];
    uid[3] = uidBuf[3];

    /* 4. Halt card to prevent repeated reads */
    uint8_t haltCmd[4];
    haltCmd[0] = PICC_HALT;
    haltCmd[1] = 0;
    RC522_CalcCRC(haltCmd, 2, &haltCmd[2]);   /* append CRC-A over 50 00 */
    uint16_t haltLen = 0;
    RC522_ToCard(PCD_TRANSCEIVE, haltCmd, 4, NULL, &haltLen);

    return 0;
}
