/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* font.h interface for reading bitmap fonts */

#ifndef UNIFONT_H
#define UNIFONT_H

#include "err.h"
#include "oku.h"

ErrCode unifont_open(const char *path_to_open, struct Unifont *new);
void    unifont_close(struct Unifont *toclose);
ErrCode unifont_render(struct Unifont *font, struct Glyph *out);


#endif /* UNIFONT_H */
