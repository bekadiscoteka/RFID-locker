#ifndef CARDS_H
#define CARDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "rc522.h"

#define CARDS_MAX	32
#define CARD_NAME_LEN	24	/* including the terminating NUL */

typedef struct {
	uint8_t uid[RC522_UID_LEN];
	char name[CARD_NAME_LEN];	/* never contains '"', '\\' or control chars */
} card_t;

/* Load the allowed cards from NVS. Call once, after nvs_flash_init(). */
void cards_init(void);

bool cards_allowed(const uint8_t uid[RC522_UID_LEN]);

/* Add a card, or rename it if it's already allowed. Saved to NVS.
 * ESP_ERR_NO_MEM if the list is full. */
esp_err_t cards_add(const uint8_t uid[RC522_UID_LEN], const char *name);

/* ESP_ERR_NOT_FOUND if the card isn't in the list. Saved to NVS. */
esp_err_t cards_remove(const uint8_t uid[RC522_UID_LEN]);

/* Copy up to max cards into out. Returns the number copied. */
size_t cards_snapshot(card_t *out, size_t max);

#endif
