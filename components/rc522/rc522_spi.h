#ifndef RC522_SPI_H
#define RC522_SPI_H

#include <stdint.h>

#include "rc522.h"

uint8_t readreg(uint8_t addr);
void writereg(uint8_t addr, uint8_t value);
void initialize_spi(const rc522_pins_t *pins);

#endif
