#ifndef UID_H
#define UID_H

#include <stdbool.h>
#include <stdint.h>

#include "rc522.h"

#define UID_HEX_LEN	(2 * RC522_UID_LEN + 1)	/* "DEADBEEF" + NUL */

/* { 0xDE, 0xAD, 0xBE, 0xEF } -> "DEADBEEF" */
void uid_to_hex(const uint8_t uid[RC522_UID_LEN], char hex[UID_HEX_LEN]);

/* "DEADBEEF" -> { 0xDE, 0xAD, 0xBE, 0xEF }. Needs exactly 2 * RC522_UID_LEN hex digits.
 * uid is written only when true is returned. */
bool uid_from_hex(const char *hex, uint8_t uid[RC522_UID_LEN]);

#endif
