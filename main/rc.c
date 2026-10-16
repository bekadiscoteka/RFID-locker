#include <stdint.h>
#include <stdio.h>

#include "pincfg.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* SPI bus */
#define SPIHOST SPI3_HOST

/* RC522 registers */
#define CommandReg      0x01
#define ComIrqReg       0x04
#define FIFODataReg     0x09
#define FIFOLevelReg    0x0A
#define WaterLevelReg   0x0B
#define BitFramingReg   0x0D
#define TxControlReg    0x14
#define VersionReg	0x37

/* RC522 commands and PICC requests */
#define IDLE_CMD	0x00
#define SOFT_RESET_CMD  0x0F
#define TRANSCEIVE_CMD  0x0C
#define REQA            0x26

static spi_device_handle_t rc522_handle;

/* Register access */
static uint8_t readreg(uint8_t addr)
{
	uint8_t formatted_addr = ((addr << 1) & ~1U) | 0x80;
	spi_transaction_t transaction = {
		.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
		.tx_data = { formatted_addr, 0x00 },
		.length = 16,
	};

	ESP_LOGI("SPI_DEBUG", "Target Reg: 0x%02X, Formatted Tx Byte: 0x%02X", addr, formatted_addr);
	ESP_ERROR_CHECK(spi_device_polling_transmit(rc522_handle, &transaction));
	return transaction.rx_data[1];
}

static void writereg(uint8_t addr, uint8_t value)
{
	uint8_t formatted_addr = (addr << 1) & ~0x80;
	spi_transaction_t transaction = {
		.flags = SPI_TRANS_USE_TXDATA,
		.tx_data = { formatted_addr, value },
		.length = 16,
	};

	ESP_ERROR_CHECK(spi_device_polling_transmit(rc522_handle, &transaction));
}

/* Reset, FIFO, and modulation helpers */
static void soft_reset(void)
{
	writereg(CommandReg, SOFT_RESET_CMD);
	ESP_LOGI("RESET WAIT", "wait for 2sec ...");
	vTaskDelay(pdMS_TO_TICKS(2000));
	ESP_LOGI("RESET WAIT", "done!");
	vTaskDelay(pdMS_TO_TICKS(2000));
}

static void commIrqReg_reset(void)
{
	writereg(ComIrqReg, 0x7F);
	vTaskDelay(pdMS_TO_TICKS(1000));
}

static void rc522_start_modulate(void)
{
	uint8_t tx_control = readreg(TxControlReg);
	writereg(TxControlReg, tx_control | 3U);
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

static void initialize_spi(void)
{
	spi_bus_config_t bus_config = {
		.mosi_io_num = MOSI,
		.miso_io_num = MISO,
		.sclk_io_num = SCLK,
		.quadhd_io_num = -1,
		.quadwp_io_num = -1,
		.max_transfer_sz = 64,
	};
	spi_device_interface_config_t device_config = {
		.mode = 0,
		.clock_speed_hz = 5 * 1000 * 1000,
		.spics_io_num = SS,
		.queue_size = 5,
	};

	ESP_ERROR_CHECK(spi_bus_initialize(SPIHOST, &bus_config, SPI_DMA_CH_AUTO));
	ESP_ERROR_CHECK(spi_bus_add_device(SPIHOST, &device_config, &rc522_handle));
}

static void rc522_init_regs(void)
{
	writereg(0x2A, 0x8D);
	writereg(0x2B, 0x3E);
	writereg(0x2C, 30);
	writereg(0x2D, 0);
	writereg(0x15, 0x40);
	writereg(0x11, 0x3D);
	rc522_start_modulate();
}

static void initialize_rc522(void)
{
	soft_reset();
	writereg(WaterLevelReg, 0x01);
	flush_fifo();

	uint8_t version = readreg(VersionReg);
	uint8_t fifo = readreg(FIFODataReg);
	uint8_t waterlevel = readreg(WaterLevelReg);
	ESP_LOGI("INITIAL", "version \tis 0x%02X", version);
	ESP_LOGI("INITIAL", "FIFODataReg[0] is 0x%02X", fifo);
	ESP_LOGI("INITIAL", "waterlevel \tis 0x%02X", waterlevel);

	rc522_init_regs();
}

/* Card polling */
static void request_card(void)
{
	flush_fifo();
	commIrqReg_reset();
	writereg(CommandReg, IDLE_CMD);
	writereg(BitFramingReg, 0x07);
	writereg(FIFODataReg, REQA);
	writereg(CommandReg, TRANSCEIVE_CMD);
	writereg(BitFramingReg, 0x87);

	vTaskDelay(pdMS_TO_TICKS(1000));

	uint8_t irq = readreg(ComIrqReg);
	if ((irq & (1U << 5)) > 0) {
		ESP_LOGI("RFID CARD", "CARD ATTACHED!!!");
		uint8_t fifo_level = readreg(FIFOLevelReg);
		printf("fifo level: %d\n", (int8_t)fifo_level);
		flush_fifo();
	}
	vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void)
{
	configure_reset_pin();
	initialize_spi();
	initialize_rc522();

	while (1) {
		request_card();
	}

}
