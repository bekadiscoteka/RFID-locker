#include <string.h>
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define SPIHOST SPI3_HOST
#define MOSI	25
#define MISO	33
#define SCLK	26
#define SS		27	
#define PIN_NUM_RST 14


/* registers */
#define MFRC522_REG_VERSION 0x37
#define MFRC522_REG_CMD		0x01
#define TX_CTRL_REG			0x14
#define	FIFO_DATA_REG		0x09 
#define WATERLEVEL_REG		0x0B

/* commands */
#define SOFT_RESET_CMD	0x0F
#define TRANSMIT_CMD	0x04
#define REQA			0x26
#define WUPA			0x52


static const char *TAG = "RC522";

static spi_device_handle_t rc522_handle;

uint8_t readreg(uint8_t addr) {
	uint8_t formaddr = ( (addr << 1) & ~1U ) | 0x80;
	ESP_LOGI("SPI_DEBUG", "Target Reg: 0x%02X, Formatted Tx Byte: 0x%02X", addr, formaddr);
	spi_transaction_t tr = {
		.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
		.tx_data = { formaddr, 0x00 },
		.length = 16,
	};
	
	ESP_ERROR_CHECK( spi_device_polling_transmit(rc522_handle, &tr) );
	return tr.rx_data[1];
}

void writereg(uint8_t addr, uint8_t value) {
	uint8_t formaddr = ( (addr << 1) & ~0x80 );
	
	spi_transaction_t tr = {
		.flags = SPI_TRANS_USE_TXDATA,
		.tx_data = { formaddr, value },
		.length = 16,
	};

	ESP_ERROR_CHECK( spi_device_polling_transmit(rc522_handle, &tr) );
}

void soft_reset(void) {
	writereg(MFRC522_REG_CMD, SOFT_RESET_CMD);
	ESP_LOGI("RESET WAIT", "wait for 2sec ...");
	vTaskDelay(pdMS_TO_TICKS(2000));
	ESP_LOGI("RESET WAIT", "done!");
	vTaskDelay(pdMS_TO_TICKS(2000));
}

void rc522_reset(void) {
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void rc522_start_modulate(void) {
	uint8_t tx_cntr = readreg(TX_CTRL_REG);
	writereg( TX_CTRL_REG, tx_cntr | 3U );	
}

void app_main(void) {

#ifdef RESET
	gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);	


	rc522_reset();

#endif

	spi_bus_config_t buscfg = {
		.mosi_io_num	= MOSI,
		.miso_io_num	= MISO,
		.sclk_io_num	= SCLK,
		.quadhd_io_num	= -1,
		.quadwp_io_num	= -1,

		.max_transfer_sz = 64,
	};

	ESP_ERROR_CHECK( spi_bus_initialize(SPIHOST, &buscfg, SPI_DMA_CH_AUTO) );

	spi_device_interface_config_t devcfg = {
		.mode				= 0,
		.clock_speed_hz		= 5 * 1000 * 1000,
		.spics_io_num		= SS,
		.queue_size			= 5,
	};

	ESP_ERROR_CHECK( spi_bus_add_device( SPIHOST, &devcfg, &rc522_handle ) );

	soft_reset();

	writereg(WATERLEVEL_REG, 0x01);

	uint8_t version		= readreg( MFRC522_REG_VERSION );
	uint8_t fifo		= readreg( FIFO_DATA_REG ); 
	uint8_t waterlevel	= readreg( WATERLEVEL_REG ); 
	ESP_LOGI( "INITIAL", "version \tis 0x%02X",			version		);
	ESP_LOGI( "INITIAL", "FIFODataReg[0] is 0x%02X",	fifo		);
	ESP_LOGI( "INITIAL", "waterlevel \tis 0x%02X",		waterlevel	); 
	rc522_start_modulate();

	uint8_t fifodata[2];
	while (1) {
		writereg( FIFO_DATA_REG, REQA );
		writereg( MFRC522_REG_CMD, TRANSMIT_CMD ); 
		uint8_t LoAlert = readreg( WATERLEVEL_REG );
		ESP_LOGI( "LO ALERT", " alert status: 0x%X", LoAlert );
		if ( (LoAlert % 2) == 0 ) {
			ESP_LOGI( "RFID CARD", "CARD ATTACHED!!!" );
			fifodata[0] = readreg(FIFO_DATA_REG);	
			fifodata[1] = readreg(FIFO_DATA_REG);	
			ESP_LOGI( "RFID CARD", "card response: 0x%X 0x%X", fifodata[1], fifodata[0] );
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
