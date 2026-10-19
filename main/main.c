#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "pincfg.h"
#include "rc522.h"
#include "wifi.h"

static const char *TAG = "app";

void app_main(void)
{
	/* NVS: needed by the WiFi driver. Erase and retry if full or from an older IDF. */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	wifi_init_sta();

	rc522_init(&(rc522_pins_t){
		.mosi = MOSI, .miso = MISO, .sclk = SCLK, .cs = SS, .rst = RST,
	});

	uint8_t uid[RC522_UID_LEN];

	while (1) {
		if (rc522_read_uid(uid) == RC_OK)
			ESP_LOGI(TAG, "UID: %02X %02X %02X %02X", uid[0], uid[1], uid[2], uid[3]);

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
