#ifndef ACCESSLOG_H
#define ACCESSLOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "rc522.h"

#define ACCESSLOG_LEN	64	/* oldest entries are overwritten */

typedef struct {
	time_t time;		/* wall clock; before SNTP sync it counts from 1970 */
	uint8_t uid[RC522_UID_LEN];
	bool granted;
} accesslog_entry_t;

/* Call once before any other accesslog_* function. */
void accesslog_init(void);

void accesslog_add(const uint8_t uid[RC522_UID_LEN], bool granted);

/* Copy up to max entries into out, newest first. Returns the number copied.
 * Safe to call from any task. */
size_t accesslog_snapshot(accesslog_entry_t *out, size_t max);

#endif
