#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#include "cards.h"

#define NVS_NS	"access"
#define NVS_KEY	"cards"

static const char *TAG = "cards";

/* Read by the RFID loop, changed by the HTTP task: every access holds lock.
 * The whole array is stored as one NVS blob. */
static card_t cards[CARDS_MAX];
static size_t count;
static SemaphoreHandle_t lock;

/* Caller holds lock. */
static int find(const uint8_t uid[RC522_UID_LEN])
{
	for (size_t i = 0; i < count; i++)
		if (memcmp(cards[i].uid, uid, RC522_UID_LEN) == 0)
			return i;
	return -1;
}

/* Write the whole list to flash. Caller holds lock. */
static esp_err_t save(void)
{
	nvs_handle_t h;
	esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
	if (err != ESP_OK)
		return err;

	if (count > 0) {
		err = nvs_set_blob(h, NVS_KEY, cards, count * sizeof cards[0]);
	} else {
		err = nvs_erase_key(h, NVS_KEY);
		if (err == ESP_ERR_NVS_NOT_FOUND)
			err = ESP_OK;
	}
	if (err == ESP_OK)
		err = nvs_commit(h);

	nvs_close(h);
	if (err != ESP_OK)
		ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
	return err;
}

/* Copy name, dropping bytes that would need escaping in JSON. Keeps UTF-8 intact. */
static void copy_name(char dst[CARD_NAME_LEN], const char *src)
{
	size_t n = 0;
	for (; *src && n < CARD_NAME_LEN - 1; src++) {
		unsigned char c = *src;
		if (c < 0x20 || c == 0x7F || c == '"' || c == '\\')
			continue;
		dst[n++] = c;
	}
	/* If we stopped inside a multi-byte character, drop its partial bytes. */
	if (*src && ((unsigned char)*src & 0xC0) == 0x80)
		while (n > 0 && ((unsigned char)dst[--n] & 0xC0) == 0x80)
			;
	dst[n] = '\0';
}

void cards_init(void)
{
	lock = xSemaphoreCreateMutex();
	configASSERT(lock);

	nvs_handle_t h;
	esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
	if (err == ESP_OK) {
		size_t len = sizeof cards;
		err = nvs_get_blob(h, NVS_KEY, cards, &len);
		nvs_close(h);
		if (err == ESP_OK && len % sizeof cards[0] == 0)
			count = len / sizeof cards[0];
	}

	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
		ESP_LOGW(TAG, "stored cards unreadable (%s), starting empty", esp_err_to_name(err));

	for (size_t i = 0; i < count; i++)
		cards[i].name[CARD_NAME_LEN - 1] = '\0';

	ESP_LOGI(TAG, "%u allowed cards", (unsigned)count);
}

bool cards_allowed(const uint8_t uid[RC522_UID_LEN])
{
	xSemaphoreTake(lock, portMAX_DELAY);
	bool found = find(uid) >= 0;
	xSemaphoreGive(lock);

	return found;
}

esp_err_t cards_add(const uint8_t uid[RC522_UID_LEN], const char *name)
{
	esp_err_t err = ESP_OK;

	xSemaphoreTake(lock, portMAX_DELAY);
	int i = find(uid);
	if (i < 0 && count == CARDS_MAX) {
		err = ESP_ERR_NO_MEM;
	} else {
		if (i < 0) {
			i = count++;
			memcpy(cards[i].uid, uid, RC522_UID_LEN);
		}
		copy_name(cards[i].name, name);
		err = save();
	}
	xSemaphoreGive(lock);

	return err;
}

esp_err_t cards_remove(const uint8_t uid[RC522_UID_LEN])
{
	esp_err_t err = ESP_ERR_NOT_FOUND;

	xSemaphoreTake(lock, portMAX_DELAY);
	int i = find(uid);
	if (i >= 0) {
		/* Shift the rest down so the list keeps its order. */
		memmove(&cards[i], &cards[i + 1], (count - i - 1) * sizeof cards[0]);
		count--;
		err = save();
	}
	xSemaphoreGive(lock);

	return err;
}

size_t cards_snapshot(card_t *out, size_t max)
{
	xSemaphoreTake(lock, portMAX_DELAY);
	size_t n = count < max ? count : max;
	memcpy(out, cards, n * sizeof cards[0]);
	xSemaphoreGive(lock);

	return n;
}
