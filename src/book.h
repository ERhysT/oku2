/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* book.h - decodes UTF-8 ebook text files */

#ifndef BOOK_H
#define BOOK_H

#include "err.h"
#include "oku.h"

/* UTF-8 file operations */
ErrCode book_open(const char *path_to_open, FILE **file_out);
void    book_close(FILE **book_to_close);

ErrCode book_get_codepoint(FILE* book_to_read, unicode *codepoint_out);
ErrCode book_unget_codepoint(FILE* book_to_write, unicode codepoint); 

/* Bookmarking (saving position to disk) */
ErrCode bookmarks_open(const char *path_to_open, FILE **file_out);
void    bookmarks_close(FILE **book_to_close);

ErrCode bookmarks_save(FILE *dest, const struct Bookmarks *src);
ErrCode bookmarks_load(FILE *src, FILE *marks, struct Bookmarks *dest);

ErrCode bookmark_new(FILE* book, struct Bookmarks *out);
ErrCode bookmark_goto(FILE *book, const struct Bookmarks *moveto);


#endif	/* BOOK_H */
