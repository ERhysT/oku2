/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* epd.h - EPD hardware interface */

#ifndef EPD_H
#define EPD_H

#include "oku_types.h"
#include "err.h"

ErrCode epd_start(struct Point *px_out);
ErrCode epd_clear(void);
ErrCode epd_refresh(void);
ErrCode epd_write(const struct Raster *img, struct Point origin);
ErrCode epd_stop(void);

#endif /* OKU_TYPES_H */
