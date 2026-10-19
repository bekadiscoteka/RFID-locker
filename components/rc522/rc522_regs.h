/*
 * MFRC522 registers, bits and commands used by this driver.
 * Names follow the datasheet so they're easy to look up.
 */
#ifndef RC522_REGS_H
#define RC522_REGS_H

#define RC522_FIFO_SIZE	64

/* Registers */
#define CommandReg      0x01
#define ComIrqReg       0x04
#define ErrorReg	0x06
#define FIFODataReg     0x09
#define FIFOLevelReg    0x0A
#define BitFramingReg   0x0D
#define ModeReg         0x11
#define TxControlReg    0x14
#define TxASKReg        0x15
#define TModeReg        0x2A
#define TPrescalerReg   0x2B
#define TReloadRegH     0x2C
#define TReloadRegL     0x2D
#define VersionReg	0x37

/* CommandReg commands */
#define IDLE_CMD	0x00
#define TRANSCEIVE_CMD  0x0C
#define SOFT_RESET_CMD  0x0F

/* ComIrqReg */
#define TxIRq_MASK	( 1U << 6 )
#define RxIRq_MASK	( 1U << 5 )
#define ErrIRq_MASK	( 1U << 1 )
#define TimerIRq_MASK	( 1U << 0 )
#define COMIRQ_CLEAR_ALL 0x7F		/* Set1 = 0: clears every IRQ bit */

/* ErrorReg */
#define CollErr_MASK	( 1U << 3 )

/* FIFOLevelReg */
#define FIFO_FLUSH	0x80		/* FlushBuffer */

/* BitFramingReg */
#define StartSend		0x80
#define TxLastBits(bits)	( (bits) & 0x07 )

/* TxControlReg */
#define Tx1RFEn		( 1U << 0 )
#define Tx2RFEn		( 1U << 1 )

/* TxASKReg */
#define TXASK_FORCE_100ASK	0x40	/* force 100% ASK modulation */

/* ModeReg */
#define MODE_TXWAITRF           0x20	/* transmitter starts only if RF field is on */
#define MODE_RESERVED           0x14	/* reserved bits, keep at reset value */
#define MODE_POLMFIN            0x08	/* MFIN pin polarity active HIGH */
#define MODE_CRCPRESET_6363     0x01	/* CRC coprocessor preset 0x6363 (ISO 14443-3) */

/* Timer: TModeReg, TPrescalerReg, TReloadReg */
#define TMODE_TAUTO             0x80	/* timer starts automatically after transmission */
#define TPRESCALER_HI           0x0D	/* upper 4 bits of 12-bit TPrescaler */
#define TPRESCALER_LO           0x3E	/* lower 8 bits of 12-bit TPrescaler */
#define TRELOAD_HI		0
#define TRELOAD_LO              30

#endif
