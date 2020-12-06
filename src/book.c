/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* book.c - decodes UTF-8 ebook text files into unicode codepoints */

#include <stdio.h>

#include "err.h"
#include "oku_types.h"

#include "book.h"

static ErrCode read_utf8_octet(FILE *book_to_read, byte *buf);
static unsigned utf8_sequence_length(byte first);
static unicode utf8tocp(byte *utf8, unsigned len);

ErrCode
book_open(const char *path_to_open, FILE **file_out)
{
    *file_out = fopen(path_to_open, "r");
    return *file_out ? SUCCESS : E_PATH;
}

/* Reads the next codepoint in the book */
ErrCode
book_getchar(FILE* book_to_read, unicode *codepoint_out)
{
    ErrCode    status;
    unsigned   utf8len, i;
    byte       utf8[4] = { 0x00, 0x00, 0x00, 0x00 };

    status = read_utf8_octet(book_to_read, utf8);
    if (status)
	goto err;

    utf8len = utf8_sequence_length(utf8[0]);
	    
    for (i=1; i<utf8len; ++i) {
	status = read_utf8_octet(book_to_read, utf8 + i);
	if (status)
	    goto err;
    }

    *codepoint_out = utf8tocp(utf8, utf8len);

#ifdef DEBUG
    printf("UTF-8: '%c' 0x%02hhX%02hhX02%hhX%02hhX -> U+%08X\n",
	   utf8[0], utf8[1], utf8[2], utf8[3], *codepoint_out,
	   (*codepoint_out & 0xFFFFFF00) ? '?' : *codepoint_out & 0xFF);
#endif

 err:
    return status;
}

void
book_close(FILE **book_to_close)
{
    if (!book_to_close || !*book_to_close)
	return; 

    fclose(*book_to_close);
    *book_to_close = NULL;

    return;
}

/* STATIC FUNCTIONS */

/* Read a single byte into buffer */
static ErrCode
read_utf8_octet(FILE *book_to_read, byte *buf)
{
    int n;

    n = fread(buf, 1, 1, book_to_read);
    if (n != 1) {
	if (feof(book_to_read)) {
	    clearerr(book_to_read);
	    return E_EOF;
	} else {
	    return E_IO;
	}
    }
    
    return SUCCESS;
}

/* Returns the number of bytes determined using the first byte of a
   UTF-8 sequence. If the sequence is not a vaild initial byte of a
   UTF-8 sequence, 0 is returned.

   The length of any UTF-8 can be determined from the five most
   significant bits of the first byte. As shown in the table below,
   where x's represent unicode codepoint data.

   length byte[0]  byte[1]  byte[2]  byte[3]
   1      0xxxxxxx
   2      110xxxxx 10xxxxxx
   3      1110xxxx 10xxxxxx 10xxxxxx
   4      11110xxx 10xxxxxx 10xxxxxx 10xxxxxx              */
static unsigned
utf8_sequence_length(byte first)
{
    const unsigned utf8_decode_length_lut[32] =
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0 };

    return utf8_decode_length_lut[first >> 3];
}

/* Decodes UTF-8 octet sequence into a unicode codepoint by removing
   sequence length and retaining only the codepoints */
static unicode
utf8tocp(byte *utf8, unsigned len)
{
    unicode codepoint = 0;

    switch ( len ) {
	/* Decode a number of bytes equal to the sequence length. */
    case 1:		
	codepoint |= *utf8 & 0x7F;
	break;
    case 2:
	codepoint |= (*utf8++ & 0x1F) << 6;
	codepoint |= (*utf8   & 0x3F);
	break;
    case 3:
	codepoint |= (*utf8++ & 0x0F) << 12;
	codepoint |= (*utf8++ & 0x3F) << 6;
	codepoint |= (*utf8   & 0x3F);
	break;
    case 4:
	codepoint |= (*utf8++ & 0x07) << 18;
	codepoint |= (*utf8++ & 0x3F) << 12;
	codepoint |= (*utf8++ & 0x3F) << 6;
	codepoint |= (*utf8   & 0x3F);
	break;
	/* Invalid UTF-8 (Fallthrough) */
    default:			
	codepoint = CODEPOINT_INVALID_CHAR;
    }

    return codepoint;
}
