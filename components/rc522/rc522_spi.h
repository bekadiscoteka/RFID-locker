/* SPI register access to the MFRC522. Private to the rc522 component. */
#ifndef RC522_SPI_H
#define RC522_SPI_H

#include <stdint.h>

#include "rc522.h"

void rc522_spi_init(const rc522_pins_t *pins);
uint8_t rc522_spi_read(uint8_t addr);
void rc522_spi_write(uint8_t addr, uint8_t value);

#endif
