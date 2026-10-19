#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "rc522.h"
#include "rc522_regs.h"
#include "rc522_spi.h"

static const char *TAG = "rc522";

#define NOW		( esp_timer_get_time() )
#define MIN(a, b)	( (a) < (b) ? (a) : (b) )

#define TRANSCEIVE_TIMEOUT_US	(50 * 1000)

/* ISO 14443-3 frames */
#define REQA		0x26
#define REQA_BITS	7	/* REQA is a 7-bit short frame */
#define ATQA_SIZE	2
#define SEL1		0x93	/* cascade level 1 */
#define ANTICOLL_NVB	0x20	/* NVB: we send 2 bytes, card sends the rest */
#define LVLSIZE		5	/* UID CLn answer: 4 UID bytes + BCC */
#define CT		0x88	/* cascade tag: UID longer than 4 bytes */


/*
 * Reset, FIFO and antenna helpers
 */

static void soft_reset(void)
{
	rc522_spi_write(CommandReg, SOFT_RESET_CMD);
	ESP_LOGI(TAG, "soft reset, waiting 4 s");
	vTaskDelay(pdMS_TO_TICKS(4000));
}

static void clear_irqs(void)
{
	rc522_spi_write(ComIrqReg, COMIRQ_CLEAR_ALL);
}

static void flush_fifo(void)
{
	rc522_spi_write(FIFOLevelReg, FIFO_FLUSH);
}

static void antenna_on(void)
{
	rc522_spi_write(TxControlReg, rc522_spi_read(TxControlReg) | Tx1RFEn | Tx2RFEn);
}

/* Copy up to rxlen bytes out of the FIFO. Returns the number copied. */
static size_t readfifo(uint8_t rx[], size_t rxlen)
{
	uint8_t fifolen = rc522_spi_read(FIFOLevelReg);

	size_t size = MIN(fifolen, rxlen);
	for (size_t i = 0; i < size; i++)
		rx[i] = rc522_spi_read(FIFODataReg);

	return size;
}


/*
 * Device setup
 */

static void hard_reset(int rst)
{
	gpio_set_level(rst, 0);
	vTaskDelay(pdMS_TO_TICKS(10));
	gpio_set_level(rst, 1);
	vTaskDelay(pdMS_TO_TICKS(50));
}

static void configure_reset_pin(int rst)
{
	if (rst < 0)
		return;

	gpio_config_t io_conf = {
		.pin_bit_mask = (1ULL << rst),
		.mode = GPIO_MODE_OUTPUT,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&io_conf));
	hard_reset(rst);
}

static void init_regs(void)
{
	rc522_spi_write(TModeReg, TMODE_TAUTO | TPRESCALER_HI);
	rc522_spi_write(TPrescalerReg, TPRESCALER_LO);
	rc522_spi_write(TReloadRegH, TRELOAD_HI);
	rc522_spi_write(TReloadRegL, TRELOAD_LO);
	rc522_spi_write(TxASKReg, TXASK_FORCE_100ASK);
	rc522_spi_write(ModeReg, MODE_TXWAITRF | MODE_RESERVED | MODE_POLMFIN | MODE_CRCPRESET_6363);
	antenna_on();
}

static void initialize_rc522(void)
{
	soft_reset();
	ESP_LOGI(TAG, "version 0x%02X", rc522_spi_read(VersionReg));
	init_regs();
}


/*
 * TRANSCEIVE: send tx to the card, then wait for its answer in the FIFO.
 * Returns RC_OK only if exactly rx_len bytes came back.
 */
static rc_status_t transceive(const uint8_t *tx, size_t tx_len, uint8_t tx_last_bits,
			      uint8_t *rx, size_t rx_len)
{
	assert(tx_len <= RC522_FIFO_SIZE);

	flush_fifo();
	clear_irqs();

	rc522_spi_write(CommandReg, IDLE_CMD);
	for (size_t i = 0; i < tx_len; i++)
		rc522_spi_write(FIFODataReg, tx[i]);
	rc522_spi_write(CommandReg, TRANSCEIVE_CMD);
	rc522_spi_write(BitFramingReg, StartSend | TxLastBits(tx_last_bits));

	rc_status_t status = RC_ERR;	/* pessimistic: hitting our own deadline is an error */
	int64_t deadline = NOW + TRANSCEIVE_TIMEOUT_US;
	do {
		uint8_t irq = rc522_spi_read(ComIrqReg);

		if (irq & ErrIRq_MASK) {
			uint8_t err = rc522_spi_read(ErrorReg);
			ESP_LOGW(TAG, "error IRQ, ErrorReg 0x%02X", err);
			if (err & CollErr_MASK) {
				readfifo(rx, rx_len);
				status = RC_COLL;
			} else {
				status = RC_ERR;
			}
			break;
		}

		if (irq & TxIRq_MASK)
			ESP_LOGV(TAG, "frame sent");

		if (irq & RxIRq_MASK) {
			status = readfifo(rx, rx_len) == rx_len ? RC_OK : RC_ERR;
			break;
		}

		if (irq & TimerIRq_MASK) {
			ESP_LOGD(TAG, "no card answered");
			status = RC_NORESP;
			break;
		}
	} while (deadline > NOW);

	return status;
}


/*
 * Card protocol: REQA -> ATQA, then ANTICOLL -> UID
 */

/* UID length from bits 7-6 of the first ATQA byte. 0 if unknown. */
static int atqa_uid_decode(uint8_t atqa0)
{
	switch (atqa0 >> 6) {
		case 0:	 return 4;
		case 1:	 return 7;
		case 2:	 return 10;
		default: ESP_LOGI(TAG, "unsupported card"); return 0;
	}
}

/* Is there a card in the field? On RC_OK, *uidlen is the card's UID length. */
static rc_status_t request_card(int *uidlen)
{
	uint8_t reqa = REQA;
	uint8_t atqa[ATQA_SIZE];

	rc_status_t status = transceive(&reqa, sizeof reqa, REQA_BITS, atqa, ATQA_SIZE);

	switch (status) {
		case RC_OK:
			*uidlen = atqa_uid_decode(atqa[0]);
			if (*uidlen == 0)
				status = RC_ERR;
			break;
		case RC_NORESP:
		case RC_ERR:
			break;
		default:
			ESP_LOGE(TAG, "request_card: unexpected status %d", status);
			break;
	}

	return status;
}

/* ANTICOLL at cascade level 1 and BCC check. 4-byte UIDs only. */
static rc_status_t read_uid4(uint8_t uid[RC522_UID_LEN])
{
	uint8_t cmd[] = { SEL1, ANTICOLL_NVB };
	uint8_t buf[LVLSIZE];

	rc_status_t status = transceive(cmd, sizeof cmd, 0, buf, LVLSIZE);
	if (status != RC_OK)
		return status;

	if (buf[0] == CT)
		return RC_ERR;

	uint8_t bcc = buf[0];
	for (size_t i = 1; i < LVLSIZE - 1; i++)
		bcc ^= buf[i];
	if (bcc != buf[LVLSIZE - 1])
		return RC_ERR;

	memcpy(uid, buf, RC522_UID_LEN);
	return RC_OK;
}


/*
 * Public API (see include/rc522.h)
 */

void rc522_init(const rc522_pins_t *pins)
{
	configure_reset_pin(pins->rst);
	rc522_spi_init(pins);
	initialize_rc522();
}

rc_status_t rc522_read_uid(uint8_t uid[RC522_UID_LEN])
{
	int uidlen = 0;

	rc_status_t status = request_card(&uidlen);
	if (status != RC_OK)
		return status;

	return read_uid4(uid);
}
