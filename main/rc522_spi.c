#include <stdint.h>
#include "pincfg.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rc522_spi.h"

#define SPIHOST SPI3_HOST

static spi_device_handle_t rc522_handle;


/* Register access */
uint8_t readreg(uint8_t addr)
{
	uint8_t formatted_addr = ((addr << 1) & ~1U) | 0x80;
	spi_transaction_t transaction = {
		.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
		.tx_data = { formatted_addr, 0x00 },
		.length = 16,
	};

	ESP_ERROR_CHECK(spi_device_polling_transmit(rc522_handle, &transaction));
	return transaction.rx_data[1];
}

void writereg(uint8_t addr, uint8_t value)
{
	uint8_t formatted_addr = (addr << 1) & ~0x80;
	spi_transaction_t transaction = {
		.flags = SPI_TRANS_USE_TXDATA,
		.tx_data = { formatted_addr, value },
		.length = 16,
	};

	ESP_ERROR_CHECK(spi_device_polling_transmit(rc522_handle, &transaction));
}

void initialize_spi(void)
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
