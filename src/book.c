/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* book.c - decodes UTF-8 ebook text files into unicode codepoints */

#include <stdio.h>
#include <assert.h>

#include "err.h"
#include "oku.h"

#include "book.h"

/* UTF-8 */
static ErrCode  read_utf8_octet(FILE *book_to_read, byte *buf);
static unsigned utf8_sequence_length(byte first);
static unicode  utf8tocp(byte *utf8, unsigned len);
static void     cptoutf8(unicode codepoint, byte (*utf8)[4]);

/* file operations */
static ErrCode flength(FILE *tomeasure, long *len_out);
static ErrCode fcksum(FILE *tohash, long *hash_out);
static void    fcloseifexists(FILE **toclose);
/* Linked list for Bookmarks */
static ErrCode new_bmlist(FILE *book, struct Bookmarks *new);
static ErrCode load_bmlist(FILE *src, struct Bookmarks *dest);

ErrCode
book_open(const char *path_to_open, FILE **file_out)
{
    assert(file_out && "Dereferenced null pointer");

    *file_out = fopen(path_to_open, "r");
    return *file_out ? SUCCESS : E_PATH;
}

/* Reads the next codepoint in the book */
ErrCode
book_get_codepoint(FILE* book_to_read, unicode *codepoint_out)
{
    ErrCode    status;
    unsigned   utf8len, i;
    byte       utf8[4] = { 0x00, 0x00, 0x00, 0x00 };

    assert(codepoint_out && book_to_read && "Dereferenced null pointer");

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

/* Rewinds the file handle back one codepoint.

   UTF-8 is a variable length encoding, so the number of bytes to
   rewind must first be calculated.  */
ErrCode
book_unget_codepoint(FILE* book_to_write, unicode codepoint) 
{
    unsigned len; 
    byte     utf8[4] = { 0x00, 0x00, 0x00, 0x00 };

    assert(book_to_write && "Dereferenced null pointer");

    cptoutf8(codepoint, &utf8);
    len = utf8_sequence_length(utf8[0]);

#ifdef DEBUG
    printf("UTF-8: '%c' 0x%02hhX%02hhX02%hhX%02hhX <- U+%08X\n",
	   utf8[0], utf8[1], utf8[2], utf8[3], codepoint,
	   (codepoint & 0xFFFFFF00) ? '?' : codepoint & 0xFF);
#endif

    return fseek(book_to_write, -1L * len, SEEK_CUR)
	? E_IO : SUCCESS;
}

void
book_close(FILE **book_to_close)
{
    return fcloseifexists(book_to_close);
}

/* BOOKMARKING INTERFACE IMPLEMENTATION */

/* Opens bookmark file for reading and writing at the start of the
   file, or creates it if it doesn't exist. */
ErrCode
bookmarks_open(const char *path_to_open, FILE **file_out)
{
    assert(path_to_open && file_out && "Dereferenced null pointer");

    *file_out = fopen(path_to_open, "w+");

    return *file_out ? SUCCESS : E_PATH;
}

/* Load any bookmarks in the bookmarks file after checking that the
   checksums match */
ErrCode
bookmarks_load(FILE *book, FILE *marks, struct Bookmarks *dest)
{
    ErrCode  status;
    long     mflen, cksum;

    assert(book && marks && dest && "Dereferenced null pointer");

    status = flength(marks, &mflen);
    if (status)
	return status;
    if (mflen == 0)		/* empty file */
	return new_bmlist(book, dest);

    if (fread(&dest->cksum, sizeof dest->cksum, 1, marks) != 1)
	return E_IO;
    status = fcksum(book, &cksum);
    if (status) {
	rewind(marks); 
	return status;
    }
    if (dest->cksum != cksum) /* files dont match */
	return new_bmlist(book, dest);
	
    return load_bmlist(book, dest);
}

ErrCode
bookmarks_save(FILE *dest, const struct Bookmarks *src)
{
    (void)dest;
    (void)src;
    return E_TODO;
}

void
bookmarks_close(FILE **toclose)
{
    return fcloseifexists(toclose);
}

/* STATIC FUNCTIONS */

/* Returns the number of bytes in a file. */
static ErrCode
flength(FILE *tomeasure, long *len_out)
{
    long start_len;

    assert(tomeasure && len_out && "Dereferenced null pointer");

    start_len = ftell(tomeasure);
    if (fseek(tomeasure, 0L, SEEK_END) == -1)
	return E_IO;
    *len_out = ftell(tomeasure);
    if (fseek(tomeasure, start_len, SEEK_SET) == -1)
	return E_IO;

    return SUCCESS;
}

/* checksum */
static ErrCode
fcksum(FILE *tohash, long *hash)
{
    //At the moment this is just the file length
    return flength(tohash, hash);
}

static void 
fcloseifexists(FILE **toclose)
{
    if (!toclose || !*toclose)
	return; 

    fclose(*toclose);
    *toclose = NULL;

    return;
}

/*
   UTF-8 ENCODING AND DECODING
*/

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

/* Encodes a single unicode codepoint as UTF-8 and stores it in an
   octet array of length 4 pointed to by utf8. Unused bytes are
   zeroed.
   
   If above the valid range, data at utf8 is unchanged.

   Codepoint Range      1B       2B       3B       4B        Codepoint x's
   U+010000 -> U+10FFFF 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx  21 bits 
   U+000800 -> U+00FFFF 1110xxxx 10xxxxxx 10xxxxxx           16 bits 
   U+000080 -> U+0007FF 110xxxxx 10xxxxxx                    11 bits
   U+000000 -> U+00007F 0xxxxxxx                             7  bits     */
static void
cptoutf8(unicode codepoint, byte (*utf8)[4])
{
    if (codepoint > 0x10FFFF) {	       /* Invalid - too long */
	return;
    } else if (codepoint > 0x00FFFF) { /* 4B */
	(*utf8)[0] = 0xFF & ((codepoint >> (21-3 )) | 0xF0);
	(*utf8)[1] = 0xFF & ((codepoint >> (21-9 )) | 0x80); 
	(*utf8)[2] = 0xFF & ((codepoint >> (21-15)) | 0x80);
	(*utf8)[3] = 0xFF & ((codepoint >> (21-21)) | 0x80);
    } else if (codepoint > 0x0007FF) { /* 3B */
	(*utf8)[0] = 0xFF & ((codepoint >> (16-4 )) | 0xE0);
	(*utf8)[1] = 0xFF & ((codepoint >> (16-10)) | 0x80);
	(*utf8)[2] = 0xFF & ((codepoint >> (16-16)) | 0x80);
    } else if (codepoint > 0x0007FF) { /* 2B */
	(*utf8)[0] = 0xFF & ((codepoint >> (11-5 )) | 0xF0);
	(*utf8)[1] = 0xFF & ((codepoint >> (11-11)) | 0x80);
    } else {			       /* 1B */
	(*utf8)[0] = 0x7F & codepoint;
    }

    return;
}

/*
   LINKED LIST IMPLEMENTATION FOR MARKS
*/

static ErrCode
new_bmlist(FILE *book, struct Bookmarks *new)
{
    (void)book;
    (void)new;
    return E_TODO;
}

static ErrCode
load_bmlist(FILE *src, struct Bookmarks *dest)
{
    (void)src;
    (void)dest;
    return E_TODO;
}
