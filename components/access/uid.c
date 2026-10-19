#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uid.h"

void uid_to_hex(const uint8_t uid[RC522_UID_LEN], char hex[UID_HEX_LEN])
{
	for (size_t i = 0; i < RC522_UID_LEN; i++)
		sprintf(hex + 2 * i, "%02X", uid[i]);
}

bool uid_from_hex(const char *hex, uint8_t uid[RC522_UID_LEN])
{
	if (strlen(hex) != 2 * RC522_UID_LEN)
		return false;
	for (size_t i = 0; i < 2 * RC522_UID_LEN; i++)
		if (!isxdigit((unsigned char)hex[i]))
			return false;

	for (size_t i = 0; i < RC522_UID_LEN; i++) {
		char byte[3] = { hex[2 * i], hex[2 * i + 1], '\0' };
		uid[i] = strtol(byte, NULL, 16);
	}
	return true;
}
