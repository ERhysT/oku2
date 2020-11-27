/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* gpio.h - Read and write digital values to BCM2835 pins */

/* Before using a gp */

#include <stddef.h>

#include "oku_types.h"
#include "err.h"

/* Input: pi->epd, Output: epd->pi */
enum GPIO_LEVEL { GPIO_LEVEL_Low = 0x00, GPIO_LEVEL_High = 0xFF };

ErrCode GPIO_start(const char *device, const char *consumer);
void GPIO_stop(void);

ErrCode GPIO_reserve_input(unsigned line);
ErrCode GPIO_reserve_output(unsigned line, enum GPIO_LEVEL initial);

ErrCode GPIO_write(unsigned line, enum GPIO_LEVEL in);
ErrCode GPIO_write_default(unsigned line);
ErrCode GPIO_read(unsigned line, enum GPIO_LEVEL *out);

/* Prints active line information if DEBUG is defined */
void GPIO_dump(void);
