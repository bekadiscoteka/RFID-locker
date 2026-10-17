#include <stdint.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "rc522_spi.h"

#define waitsec(sec)	( vTaskDelay(pdMS_TO_TICKS( (sec) * 1000)) )
#define NOW		( esp_timer_get_time() )

#define MIN(a, b)	( a <= b ? a : b )
#define MAX(a, b)	( a >= b ? a : b )


/* RC522 registers */
#define CommandReg      0x01
#define ComIrqReg       0x04
#define FIFODataReg     0x09
#define FIFOLevelReg    0x0A
#define WaterLevelReg   0x0B
#define BitFramingReg   0x0D
#define ModeReg         0x11
#define TxControlReg    0x14
#define TxASKReg        0x15
#define TModeReg        0x2A
#define TPrescalerReg   0x2B
#define TReloadRegH     0x2C
#define TReloadRegL     0x2D
#define VersionReg	0x37

/* RC522 register bits and values */
#define TMODE_TAUTO             0x80	/* timer starts automatically after transmission */
#define TPRESCALER_HI           0x0D	/* upper 4 bits of 12-bit TPrescaler */
#define TPRESCALER_LO           0x3E	/* lower 8 bits of 12-bit TPrescaler */
#define TRELOAD_HI		0 
#define TRELOAD_LO              30
#define TXASK_FORCE_100ASK      0x40	/* force 100% ASK modulation */
#define MODE_TXWAITRF           0x20	/* transmitter starts only if RF field is on */
#define MODE_RESERVED           0x14	/* reserved bits, keep at reset value */
#define MODE_POLMFIN            0x08	/* MFIN pin polarity active HIGH */
#define MODE_CRCPRESET_6363     0x01	/* CRC coprocessor preset 0x6363 (ISO 14443-3) */

/* BitFraming flags */

#define StartSend		0x80
#define	RxAlign(align)		( align << 4 )
#define TxLastBits(bits)	( bits ) 

/* RC522 commands and PICC requests */
#define IDLE_CMD	0x00
#define SOFT_RESET_CMD  0x0F
#define TRANSCEIVE_CMD  0x0C

#define REQA            0x26
#define ATQA_SIZE	2
#define SEL1		0x93
#define SEL2		0x95
#define SEL3		0x97


/* Reset, FIFO, and modulation helpers */
static void soft_reset(void)
{
	writereg(CommandReg, SOFT_RESET_CMD);
	ESP_LOGI("RESET WAIT", "waitsec for 2sec ...");
	vTaskDelay(pdMS_TO_TICKS(2000));
	ESP_LOGI("RESET WAIT", "done!");
	vTaskDelay(pdMS_TO_TICKS(2000));
}

static void commIrqReg_reset(void)
{
	writereg(ComIrqReg, 0x7F);
	//vTaskDelay(pdMS_TO_TICKS(1000));
}

#define Tx1RFEn 1U
#define Tx2RFEn 2U
static void start_modulate(void)
{
	writereg(TxControlReg, readreg(TxControlReg) | Tx1RFEn | Tx2RFEn);
}

static void flush_fifo(void)
{
	writereg(FIFOLevelReg, 0x80);
}

/* Device setup */
#ifdef HARDRESET
static void rc522_reset(void)
{
	gpio_set_level(RST, 0);
	vTaskDelay(pdMS_TO_TICKS(10));
	gpio_set_level(RST, 1);
	vTaskDelay(pdMS_TO_TICKS(50));
}
#endif

static void configure_reset_pin(void)
{
#ifdef HARDRESET
	gpio_config_t io_conf = {
		.pin_bit_mask = (1ULL << RST),
		.mode = GPIO_MODE_OUTPUT,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&io_conf));
	rc522_reset();
#endif
}


static void init_regs(void)
{
	writereg(TModeReg, TMODE_TAUTO | TPRESCALER_HI);
	writereg(TPrescalerReg, TPRESCALER_LO);
	writereg(TReloadRegH, TRELOAD_HI);
	writereg(TReloadRegL, TRELOAD_LO);
	writereg(TxASKReg, TXASK_FORCE_100ASK);
	writereg(ModeReg, MODE_TXWAITRF | MODE_RESERVED | MODE_POLMFIN | MODE_CRCPRESET_6363);
	start_modulate();
}

static void initialize_rc522(void)
{
	soft_reset();

	uint8_t version = readreg(VersionReg);
	ESP_LOGI("INFO", "version \tis 0x%02X", version);

	init_regs();
}

/*
 *	TRANSCEIVE COMMAND:
 *	transmits your data with reader to environment
 *	and then automatically waits for response
 *	
 *	flushes FIFO, resets ComIRqReg 
*/
#define FIFOSIZE UINT8_MAX

#define TxIRq_MASK	( 1U << 6 )
#define RxIRq_MASK	( 1U << 5 )

#define TimerIRq_MASK	( 1U )
#define ErrIRq_MASK	( 2U )

typedef enum {RC_OK, RC_NORESP, RC_ERR, RC_WARN} rc_status_t;

static rc_status_t transceive(const uint8_t *tx, size_t tx_len, uint8_t tx_last_bits,
			uint8_t *rx, size_t rx_len) {
	assert( tx_len <= FIFOSIZE );

	flush_fifo();
	commIrqReg_reset();
	
	writereg(CommandReg, IDLE_CMD);
	for (size_t i=0; i < tx_len; i++) 
		writereg( FIFODataReg, *(tx+i) );
	writereg( CommandReg,	TRANSCEIVE_CMD);
	writereg( BitFramingReg, StartSend | TxLastBits(tx_last_bits));
	
	rc_status_t status = RC_ERR; /* pessimistic approach */ 
	int64_t deadline = NOW + (50 * 1000);
	do {
		uint8_t irq = readreg(ComIrqReg);

		if (irq & ErrIRq_MASK) {
			ESP_LOGE("ERROR IRq", "Error occured");

		status = RC_ERR;
			break;
		}

		if (irq & TxIRq_MASK) 
			ESP_LOGV("TRANSCEIVE", "Transaction was sent successfully");

		if (irq & RxIRq_MASK) {
		status = RC_OK;

			uint8_t fifolen = readreg(FIFOLevelReg);
			if (fifolen > rx_len) {
			status = RC_WARN;
				ESP_LOGW("TRANSCEIVE", "rx_len is smaller than the received data's length");
			}

			for (size_t i=0; i < MIN(fifolen, rx_len); i++) 
				rx[i] = readreg( FIFODataReg );

			break;
		}

		if (irq & TimerIRq_MASK) {
			ESP_LOGD("READER TIMER", "Reader didn't find any device");
		status = RC_NORESP;
			break;
		}
	} 
	while ( deadline > NOW );
	
	return status;
}

/* decode 7-6 bits of first ATQA byte */
static int atqa_uid_decode(uint8_t atqa0) {
	
	uint8_t uidlen_code = atqa0 >> 6;

	size_t uidlen = 0;
	switch (uidlen_code) {
		case 0: uidlen=4; break;
		case 1: uidlen=7; break;
		case 2: uidlen=10; break;
		default: ESP_LOGI("CARD", "Unsupported card"); break;
	}
	return uidlen;
}

/* 
 * request for answer from environment (is there anybody here?) 
*/
static int request_card(int *uidlen)
{
	uint8_t reqa = REQA;		
	uint8_t atqa[2];

	rc_status_t status = transceive( &reqa, 1, 7, atqa, 2 );

	switch (status) {
		case RC_WARN:	ESP_LOGW("request_card", "pay attention to last warnings from transceive!"); break;
		case RC_OK:	*uidlen = atqa_uid_decode(atqa[0]); break;
		case RC_NORESP: break;
		case RC_ERR: 	break;	
		default:	ESP_LOGE("request_card", "Unsupported status: %d", status); break;
	}

	return status; 
}

#define BCC 5
static rc_status_t read_uid(uint8_t uid[]) {
	uint8_t anticoll[2] = { SEL1, 0x20 };
	rc_status_t status = transceive(anticoll, 2, 0, uid, BCC);	
	if (status == RC_ERR) {
		ESP_LOGE("UID READ", "Potential collision");
		return status;
	}
	
	uint8_t xor = uid[0];
	for (int i=1; i < BCC-1; i++)
		xor ^= uid[i];	
	
	if (xor == uid[BCC-1]) 
		ESP_LOGI("BCC STATUS", "BCC successfully passed");
	return RC_OK;
}

void app_main(void)
{
	configure_reset_pin();
	initialize_spi();
	initialize_rc522();

	int uidlen;
	uint8_t uid[5];
	rc_status_t status;

	while (1) {
		status = request_card(&uidlen);
		if (status == RC_OK) {
			ESP_LOGI("ATQA response", "UID size: %d bytes", uidlen);

			status = read_uid(uid);
			if (status == RC_OK) 
				ESP_LOGI("UID", "detected UID: %02X %02X %02X %02X", uid[0], uid[1], uid[2], uid[3]);
		}
		

		waitsec(1);
	}

}
