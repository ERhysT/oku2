/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* book.h - decodes UTF-8 ebook text files */

#ifndef BOOK_H
#define BOOK_H

#include "err.h"
#include "oku.h"

/* UTF-8 file operations */
ErrCode book_open(const char *path_to_open, struct Book *new);
void    book_close(struct Book *toclose);

ErrCode book_get_codepoint(struct Book *toread, unicode *codepoint_out);
ErrCode book_unget_codepoint(struct Book *towrite, unicode codepoint); 

/* Bookmarking (saving position to disk) */
ErrCode bookmarks_open(const struct Book *opened, struct Bookmarks *out);
void    bookmarks_close(struct Bookmarks *toclose);

ErrCode bookmarks_push(const struct Book *position, struct Bookmarks *addto);

#endif	/* BOOK_H */
