/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* epd.h - EPD hardware interface */

#ifndef EPD_H
#define EPD_H

#include "oku_types.h"
#include "err.h"

ErrCode epd_start(void);
ErrCode epd_clear(void);
ErrCode epd_write(const byte *bmp, size_t len); 
ErrCode epd_refresh(void);
ErrCode epd_stop(void);

#endif /* OKU_TYPES_H */
