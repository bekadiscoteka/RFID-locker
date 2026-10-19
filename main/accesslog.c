#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "accesslog.h"

/* Ring buffer. Written by the RFID loop, read by the HTTP task: every access holds lock. */
static accesslog_entry_t ring[ACCESSLOG_LEN];
static size_t head;	/* next slot to write */
static size_t count;	/* valid entries, at most ACCESSLOG_LEN */
static SemaphoreHandle_t lock;

void accesslog_init(void)
{
	lock = xSemaphoreCreateMutex();
	configASSERT(lock);
}

void accesslog_add(const uint8_t uid[RC522_UID_LEN], bool granted)
{
	accesslog_entry_t e = { .time = time(NULL), .granted = granted };
	memcpy(e.uid, uid, RC522_UID_LEN);

	xSemaphoreTake(lock, portMAX_DELAY);
	ring[head] = e;
	head = (head + 1) % ACCESSLOG_LEN;
	if (count < ACCESSLOG_LEN)
		count++;
	xSemaphoreGive(lock);
}

size_t accesslog_snapshot(accesslog_entry_t *out, size_t max)
{
	xSemaphoreTake(lock, portMAX_DELAY);
	size_t n = count < max ? count : max;
	for (size_t i = 0; i < n; i++)
		out[i] = ring[(head + ACCESSLOG_LEN - 1 - i) % ACCESSLOG_LEN];
	xSemaphoreGive(lock);

	return n;
}
