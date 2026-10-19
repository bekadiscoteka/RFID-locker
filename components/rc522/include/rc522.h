#ifndef RC522_H
#define RC522_H

#include <stdint.h>

#define RC522_UID_LEN	4	/* only single-size (4-byte) UIDs are supported */

typedef enum {
	RC_OK,		/* card answered, data received and checked */
	RC_NORESP,	/* no card in the field */
	RC_ERR,		/* bad data, unsupported card, or reader not responding */
	RC_WARN,
	RC_COLL,	/* more than one card answered */
} rc_status_t;

typedef struct {
	int mosi, miso, sclk, cs;
	int rst;		/* -1 if the RST pin is not connected */
} rc522_pins_t;

/* Set up SPI, reset the reader and switch the antenna on. */
void rc522_init(const rc522_pins_t *pins);

/* Look for a card and read its UID (REQA + ANTICOLL + BCC check).
 * uid is written only when RC_OK is returned. */
rc_status_t rc522_read_uid(uint8_t uid[RC522_UID_LEN]);

#endif
