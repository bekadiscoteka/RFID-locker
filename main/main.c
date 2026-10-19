#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "access.h"
#include "accesslog.h"
#include "cards.h"
#include "pincfg.h"
#include "rc522.h"
#include "web.h"
#include "wifi.h"

#define POLL_MS		250
#define REPEAT_US	(3000 * 1000)	/* same card again within this time: ignored */

static const char *TAG = "app";

/* NVS holds the WiFi driver's data and our card list. Erase and retry if full or from an older IDF. */
static void nvs_init(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
}

/* Poll the reader forever: decide on each new card and hand the decision to access_event(). */
static void rfid_loop(void)
{
	uint8_t uid[RC522_UID_LEN];
	uint8_t last_uid[RC522_UID_LEN] = { 0 };
	int64_t last_seen = 0;

	while (1) {
		if (rc522_read_uid(uid) == RC_OK) {
			/* A card held on the reader is read again and again: report it once. */
			int64_t now = esp_timer_get_time();
			bool repeat = memcmp(uid, last_uid, RC522_UID_LEN) == 0 &&
				      now - last_seen < REPEAT_US;
			memcpy(last_uid, uid, RC522_UID_LEN);
			last_seen = now;

			if (!repeat)
				access_event(uid, cards_allowed(uid));
		}

		vTaskDelay(pdMS_TO_TICKS(POLL_MS));
	}
}

void app_main(void)
{
	/* 1. Storage and app state */
	nvs_init();
	accesslog_init();
	cards_init();
	access_init(LED_PIN, OUT_PIN);

	/* 2. Network: WiFi, clock (log times count from 1970 until SNTP syncs), web UI */
	wifi_init_sta();
	esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
	ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp));
	web_start();

	/* 3. Reader */
	rc522_init(&(rc522_pins_t){
		.mosi = MOSI, .miso = MISO, .sclk = SCLK, .cs = SS, .rst = RST,
	});

	ESP_LOGI(TAG, "ready, waiting for cards");
	rfid_loop();
}
