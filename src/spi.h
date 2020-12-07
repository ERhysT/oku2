/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* spi.h - SPI communication using spidev.
 *
 * SPI Clock Mode 0 (CPHA = 0, CPOL = 0).
 * Chip select line is not altered and must be configured manually.
 * Transmits 8 bits per word MSB first.
 * If COMMS_DEBUG is defined, each byte written is dumped to stdout.
 * 
 */

#include <stddef.h>

#include "oku.h"
#include "err.h"

ErrCode SPI_start(const char *device, uint64_t speed_mhz);
void    SPI_stop(void);
ErrCode SPI_write_byte(byte tx);
