#ifndef ACCESS_H
#define ACCESS_H

#include <stdbool.h>
#include <stdint.h>

#include "rc522.h"

/* Configure the LED and output pins (both start low). Call accesslog_init() first. */
void access_init(int led_pin, int out_pin);

/*
 * React to a decision made by the caller: log it, show it on the LED and,
 * if granted, pulse the output pin for ~3 s. Returns immediately.
 * Call from one task only.
 */
void access_event(const uint8_t uid[RC522_UID_LEN], bool granted);

#endif
