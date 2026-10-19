#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#include "wifi.h"

static const char *TAG = "wifi";

/* Debug: list visible networks once on boot. Set to 0 when WiFi works. */
#define WIFI_SCAN_ON_BOOT 0

/* True while the boot scan runs; connecting at the same time would fail the scan. */
static volatile bool scanning;

/* Runs in the event loop task, not in app_main's task: keep it short, never block. */
static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
	if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
		if (!scanning)
			esp_wifi_connect();
	} else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
		wifi_event_sta_disconnected_t *e = data;
		ESP_LOGW(TAG, "disconnected (reason %d), retrying", e->reason);
		esp_wifi_connect();
	} else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *e = data;
		ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&e->ip_info.ip));
	}
}

static void scan_print(void)
{
	static wifi_ap_record_t recs[20];
	uint16_t n = 20;
	wifi_scan_config_t sc = { .show_hidden = true };

	ESP_ERROR_CHECK(esp_wifi_scan_start(&sc, true)); /* blocks ~2 s */
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&n, recs));

	ESP_LOGI(TAG, "scan: %u networks", n);
	for (int i = 0; i < n; i++)
		ESP_LOGI(TAG, "  %-32s rssi %4d  ch %2d  auth %d",
			 (char *)recs[i].ssid, recs[i].rssi, recs[i].primary, recs[i].authmode);
}

void wifi_init_sta(void)
{
	/* The network stack is needed even without WiFi: SNTP and the HTTP server use it. */
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	if (CONFIG_APP_WIFI_SSID[0] == '\0') {
		ESP_LOGE(TAG, "no network set: idf.py menuconfig -> WiFi station");
		return;
	}

	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	/* Register before esp_wifi_start(), or STA_START is missed. */
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));

	wifi_config_t wc = {
		.sta = {
			.ssid = CONFIG_APP_WIFI_SSID,
			.password = CONFIG_APP_WIFI_PASSWORD,
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
	scanning = WIFI_SCAN_ON_BOOT;
	ESP_ERROR_CHECK(esp_wifi_start());
	if (scanning) {
		scan_print();
		scanning = false;
		esp_wifi_connect();
	}

	ESP_LOGI(TAG, "started, connecting to \"%s\"", CONFIG_APP_WIFI_SSID);
}
