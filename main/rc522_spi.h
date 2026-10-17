#ifndef RC522_SPI
#define RC522_SPI

#include <stdint.h>

#define SPIHOST SPI3_HOST

uint8_t readreg(uint8_t addr);
void writereg(uint8_t addr, uint8_t value);
void initialize_spi(void);

#endif
