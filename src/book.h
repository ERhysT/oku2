/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* book.h - decodes UTF-8 ebook text files */

#ifndef BOOK_H
#define BOOK_H

#include "err.h"
#include "oku_types.h"

/* Opening UTF-8 encoded file */
ErrCode book_open(const char *path_to_open, FILE **file_out);
ErrCode book_getchar(FILE* book_to_read, unicode *codepoint_out);
void book_close(FILE **book_to_close);

#endif	/* BOOK_H */
