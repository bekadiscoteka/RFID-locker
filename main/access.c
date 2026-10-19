#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "access.h"
#include "accesslog.h"

#define PULSE_US	(3000 * 1000)	/* output high time on grant */
#define BLINK_US	(300 * 1000)	/* LED blink on deny */

static const char *TAG = "access";

static int led, out;
static esp_timer_handle_t off_timer;
static volatile bool pulsing;	/* output is high; cleared by the timer */

/* Runs in the esp_timer task when the pulse or blink ends. */
static void all_off(void *arg)
{
	gpio_set_level(out, 0);
	gpio_set_level(led, 0);
	pulsing = false;
}

/* (Re)start the off timer; a second grant during a pulse extends it to a full 3 s. */
static void off_after(uint64_t us)
{
	esp_timer_stop(off_timer);	/* fails harmlessly if not running */
	ESP_ERROR_CHECK(esp_timer_start_once(off_timer, us));
}

void access_init(int led_pin, int out_pin)
{
	led = led_pin;
	out = out_pin;

	gpio_config_t io = {
		.pin_bit_mask = (1ULL << led) | (1ULL << out),
		.mode = GPIO_MODE_OUTPUT,
	};
	ESP_ERROR_CHECK(gpio_config(&io));
	gpio_set_level(led, 0);
	gpio_set_level(out, 0);

	esp_timer_create_args_t args = { .callback = all_off, .name = "access_off" };
	ESP_ERROR_CHECK(esp_timer_create(&args, &off_timer));
}

void access_event(const uint8_t uid[RC522_UID_LEN], bool granted)
{
	accesslog_add(uid, granted);
	ESP_LOGI(TAG, "%02X%02X%02X%02X %s", uid[0], uid[1], uid[2], uid[3],
		 granted ? "GRANTED" : "denied");

	if (granted) {
		pulsing = true;
		gpio_set_level(out, 1);
		gpio_set_level(led, 1);
		off_after(PULSE_US);
	} else if (!pulsing) {
		/* Don't cut a running grant pulse short just to blink. */
		gpio_set_level(led, 1);
		off_after(BLINK_US);
	}
}
