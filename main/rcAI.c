#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "RC522";

// Pin definitions (Using SPI3 / VSPI IO_MUX pins)
#define PIN_NUM_MISO 33
#define PIN_NUM_MOSI 25
#define PIN_NUM_CLK  26
#define PIN_NUM_CS   27
#define PIN_NUM_RST  14

// MFRC522 Registers & Commands
#define MFRC522_REG_VERSION 0x37

static spi_device_handle_t rc522_spi_handle;

// Read a single byte from an MFRC522 register
uint8_t rc522_read_reg(uint8_t reg_addr) {
    // Format address: Bit 7 set to 1 for Read, bits 6-1 contain address
    uint8_t addr_byte = ((reg_addr << 1) & 0x7E) | 0x80;
    
    // Polling transfer for quick, low-latency 2-byte transfer
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        .length = 16,                        // 16 bits: 1 address byte + 1 dummy/data byte
        .tx_data = { addr_byte, 0x00 }       // Transmit register address, then dummy byte
    };

    esp_err_t ret = spi_device_polling_transmit(rc522_spi_handle, &t);
    ESP_ERROR_CHECK(ret);

    return t.rx_data[1]; // Received byte in response to dummy byte
}

// Write a single byte to an MFRC522 register
void rc522_write_reg(uint8_t reg_addr, uint8_t val) {
    // Format address: Bit 7 set to 0 for Write
    uint8_t addr_byte = (reg_addr << 1) & 0x7E;

    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 16,                        // 16 bits: 1 address byte + 1 value byte
        .tx_data = { addr_byte, val }
    };

    esp_err_t ret = spi_device_polling_transmit(rc522_spi_handle, &t);
    ESP_ERROR_CHECK(ret);
}

// Perform hardware reset on the RC522
void rc522_reset(void) {
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void app_main(void) {
    // 1. Configure Hardware Reset Pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 2. Initialize SPI Bus (SPI3 Host)
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    // 3. Register RC522 Device to SPI Bus
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5 * 1000 * 1000,   // 5 MHz clock speed
        .mode = 0,                           // SPI Mode 0 (CPOL=0, CPHA=0)
        .spics_io_num = PIN_NUM_CS,          // Hardware CS pin
        .queue_size = 7                      // Queue depth
    };
    ret = spi_bus_add_device(SPI3_HOST, &devcfg, &rc522_spi_handle);
    ESP_ERROR_CHECK(ret);

    // 4. Hardware Reset & Read Chip Version
    rc522_reset();

    uint8_t version = rc522_read_reg(MFRC522_REG_VERSION);
    ESP_LOGI(TAG, "MFRC522 Version Register Raw Value: 0x%02X", version);

    if (version == 0x92) {
        ESP_LOGI(TAG, "Successfully detected MFRC522 (Version 2.0)");
    } else if (version == 0x91) {
        ESP_LOGI(TAG, "Successfully detected MFRC522 (Version 1.0)");
    } else {
        ESP_LOGE(TAG, "Failed to communicate with MFRC522! Check wiring.");
    }
}
