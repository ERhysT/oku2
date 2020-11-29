/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* book.c - decodes UTF-8 ebook text files into unicode codepoints */

#include <stdio.h>

#include "err.h"
#include "oku_types.h"

ErrCode
book_open(const char *path_to_open, FILE **file_out)
{
    *file_out = fopen(path_to_open, "r");
    return *file_out  ? SUCCESS : E_IO;
}

/* Reads the next codepoint in the book */
ErrCode
book_getchar(const FILE* book_to_read, codept *char_out)
{
    *char_out = 'Q';
    return SUCCESS;
}

void
book_close(FILE **book_to_close)
{
    fclose(*book_to_close);
    *book_to_close = NULL;
    return;
}
