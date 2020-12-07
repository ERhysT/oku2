/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* book.h - decodes UTF-8 ebook text files */

#ifndef BOOK_H
#define BOOK_H

#include "err.h"
#include "oku.h"

/* Opening and closing UTF-8 encoded file */
ErrCode book_open(const char *path_to_open, FILE **file_out);
void    book_close(FILE **book_to_close);

/* Unicode operations */
ErrCode book_get_codepoint(FILE* book_to_read, unicode *codepoint_out);
ErrCode book_unget_codepoint(FILE* book_to_write, unicode codepoint); 

/* Bookmarking */
ErrCode book_new_mark(FILE* book, struct Bookmark *out);
ErrCode book_apply_mark(FILE *book, const struct Bookmark *moveto);
ErrCode book_save_mark(FILE *dest, const struct Bookmark *src);
ErrCode book_load_mark(FILE *src, struct Bookmark *dest);

#endif	/* BOOK_H */
