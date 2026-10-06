#include <stdio.h>
#include <string.h>
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/spi_types.h"


#define GPIO_MOSI 25
#define GPIO_MISO 33
#define GPIO_SCLK 26
#define GPIO_CS 23

#define VSPI_HOST SPI3_HOST

void SPI_init(spi_host_device_t host) {

	spi_bus_config_t bus_cfg = {
		.mosi_io_num = GPIO_MOSI,
		.miso_io_num = GPIO_MISO,
		.sclk_io_num = GPIO_SCLK,
		.quadhd_io_num = -1,
		.quadwp_io_num = -1
	};

	ESP_ERROR_CHECK( spi_bus_initialize(host, &bus_cfg, SPI_DMA_CH_AUTO) );
}

spi_device_handle_t SPI_add_dev(spi_host_device_t host, int cs) {

	spi_device_interface_config_t devcfg = {
		.command_bits	= 0,
		.address_bits	= 0,
		.dummy_bits		= 0,
		.clock_speed_hz = 2000000,
		.duty_cycle_pos	= 128,
		.mode			= 0,
		.spics_io_num	= cs,
		.queue_size		= 3
	};

	spi_device_handle_t spi_handle;

	ESP_ERROR_CHECK( spi_bus_add_device(host, &devcfg, &spi_handle) );

	return spi_handle;

}


void SPI_transmit( spi_device_handle_t hp, char *data, int bytes ) {
	spi_transaction_t t;
	memset(&t, 0, sizeof(spi_transaction_t));

	t.tx_buffer = data;
	t.length	= bytes * 8;	

	ESP_ERROR_CHECK( spi_device_transmit(hp, &t) );
}

void app_main(void)
{
	SPI_init(VSPI_HOST);
	spi_device_handle_t dh = SPI_add_dev(VSPI_HOST, GPIO_CS); 

	char sayhi[] = "hi, there!";
	while (1) {
		SPI_transmit(dh, sayhi, strlen(sayhi));
		vTaskDelay(pdMS_TO_TICKS(10000));
	}
}
