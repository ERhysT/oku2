/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* book.c - decodes UTF-8 ebook text files into unicode codepoints */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "err.h"
#include "oku.h"

#include "book.h"

#define STACK_FEXT        ".oku" /* stack save file extension */
#define STACK_INITIAL     10

/* UTF-8 */
static ErrCode   fread_utf8_octet(FILE *book_to_read, byte *buf);
static unsigned  utf8_sequence_length(byte first);
static unicode   utf8tocp(byte *utf8, unsigned len);
static void      cptoutf8(unicode codepoint, byte (*utf8)[4]);

/* file operations */
static ErrCode  flrc(FILE *fh, checksum *lrc, size_t *len);
static char    *fname_create(checksum name, const char *ext);
static void     fcloseifexists(FILE **toclose);

/* bookmarking stack and io */
static ErrCode  load_bmstack(struct Bookmarks *out);
static ErrCode  save_bmstack(struct Bookmarks *out);
static ErrCode  grow_bmstack(long *stack, size_t newlen);

/* Generate a book object from filepath */
ErrCode
book_open(const char *path, struct Book *new)
{
    ErrCode status;

    assert_ptr(path != NULL);

    new->fh = fopen(path, "r");
    if (!new->fh)
	return E_PATH;

    status = flrc(new->fh, &new->fhash, &new->len);    

    return status;
}

/* Reads the next codepoint in the book */
ErrCode
book_get_codepoint(struct Book *toread, unicode *codepoint_out)
{
    ErrCode    status;
    unsigned   utf8len, i;
    byte       utf8[4] = { 0x00, 0x00, 0x00, 0x00 };

    assert_ptr(codepoint_out && toread && toread->fh);

    status = fread_utf8_octet(toread->fh, utf8);
    if (status)
	goto err;

    utf8len = utf8_sequence_length(utf8[0]);
	    
    for (i=1; i<utf8len; ++i) {
	status = fread_utf8_octet(toread->fh, utf8 + i);
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
book_unget_codepoint(struct Book *writeto, unicode codepoint) 
{
    unsigned len; 
    byte     utf8[4] = { 0x00, 0x00, 0x00, 0x00 };

    assert_ptr(writeto && writeto->fh);

    cptoutf8(codepoint, &utf8);
    len = utf8_sequence_length(utf8[0]);

#ifdef DEBUG
    printf("UTF-8: '%c' 0x%02hhX%02hhX02%hhX%02hhX <- U+%08X\n",
	   utf8[0], utf8[1], utf8[2], utf8[3], codepoint,
	   (codepoint & 0xFFFFFF00) ? '?' : codepoint & 0xFF);
#endif

    return fseek(writeto->fh, -1L * len, SEEK_CUR)
	? E_IO : SUCCESS;
}

void
book_close(struct Book *toclose)
{
    fcloseifexists(&toclose->fh);
}

/* BOOKMARKING INTERFACE IMPLEMENTATION */

/* Loads bookmarks (position stack) from disk using the opened
   book's hash. If this is a new book with no bookmarks, an empty one
   is allocated. */
ErrCode
bookmarks_open(const struct Book *opened, struct Bookmarks *new)
{
    new->n     = 0;
    new->len   = STACK_INITIAL;
    new->stack = calloc(new->len, sizeof *new->stack);
    if (!new->stack)
	return E_MEM;
    new->fname = fname_create(opened->fhash, STACK_FEXT);
    if (!new->fname)
	return E_MEM;

    /* If this book has been opened before there should be a matching
       structure saved to disk */
    new->fh = fopen(new->fname, "r+");
    if (!new->fh) {		/* new book, create new file */
	err_clear_errno();
	new->fh = fopen(new->fname, "w+");
	return new->fh ? SUCCESS : E_IO;
    } 

    return load_bmstack(new);	/* loads previously saved data */
}

/* Pushes a new bookmark to the bookmark stack */
ErrCode
bookmarks_push(const struct Book *position, struct Bookmarks *addto)
{
    ErrCode status;
    /* Grow stack if full */
    if (addto->n == addto->len) {
	addto->len *= 2;
	status = grow_bmstack(addto->stack, addto->len);
	if (status)
	    return status;
    }
    
    addto->stack[addto->n] = ftell(position->fh);
    if (addto->stack[addto->n] == -1)
	return E_IO;
    else
	++addto->n;

#ifdef DEBUG
    printf("BMstack: push @%ldB\t(%04u,%04u) -> %s\n",
	   addto->stack[addto->n-1], addto->n-1, addto->len, addto->fname);
#endif

    return SUCCESS;
}
    

/* Saves any bookmarks in the stack to disk and and frees all
   dynamically allocated memory associated with the structure.  */
void
bookmarks_close(struct Bookmarks *toclose)
{
    assert_ptr(toclose!=NULL);

    if (toclose->fh) {
	if (toclose->n > 0) 	/* stack populated: write to file  */
	    save_bmstack(toclose);
	fclose(toclose->fh);
	if (toclose->n == 0)	/* stack empty: delete empty file */
	    remove(toclose->fname);
    }
    if (toclose->stack)
	free(toclose->stack);
    if (toclose->fname)
	free(toclose->fname);

    return; 
}

/* STATIC FUNCTIONS */

/* Performs 2B XOR checksum on the file stream fh from its current
   position to the end of the file.

   If EOF is reached SUCCESS is returned, lrc and len are populated
   with the checksum and file length respectively. The file
   position set to the beginning of the file.

   Returns E_IO if EOF not reached. */
static ErrCode
flrc(FILE *fh, checksum *lrc, size_t *len)
{
    int ch[2];			

    *lrc = 0;
    *len = 0; 
    while ((ch[0]=getc(fh)) != EOF) {
	*lrc ^= ch[0] << 8;
	++len;
	if ((ch[1]=getc(fh)) != EOF) {
	    *lrc ^= ch[1];
	    ++len;
	} else {
	    break;
	}
    }
	
    return feof(fh) ? (rewind(fh), SUCCESS) : E_IO;
}

/* Returns a dynamically allocated filename created from a hexidecimal
   representation of the checksum and the file extension. */
char *
fname_create(checksum name, const char *ext)
{
    assert(sizeof name == 2);

    char     *str;		/* filename string */
    ssize_t   len;		/* number of characters in string */

    len = (sizeof name * 2) + strlen(ext);
    str = malloc(len+1);	/* extra byte for null character */
    if (!str)			
	return NULL;

    if (snprintf(str, len+1, "%04x%s", name, ext) != len) 
	return NULL;

    return str;
}

/* Closes file and sets stream ptr to null - or does nothing if
   already null. */
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
fread_utf8_octet(FILE *fh, byte *buf)
{
    int n;
    assert_ptr(fh && buf);

    n = fread(buf, 1, 1, fh);
    if (n != 1) {
	if (feof(fh)) {
	    clearerr(fh);
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
    assert_ptr(utf8 != NULL);

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
    assert_ptr(utf8 != NULL);

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

/* BOOKMARKING STACK AND IO */

/* Reads a stack saved to disk using save_bmstack() into the bookmark
   handle arg. 

   File format:    | n | len | stack | 

   Returns: SUCCESS    struct populated as expected
            E_FFORMAT  EOF reached unexpectedly or corrupt data
            E_IO       file read error
            E_MEM      malloc error
*/
static ErrCode
load_bmstack(struct Bookmarks *out)
{
    ErrCode status;
    /* 
       Determine how much to read from file
    */
    if (fread(&out->n, sizeof out->n, 1, out->fh) != 1)
	goto check_eof;
    if (fread(&out->len, sizeof out->len, 1, out->fh) != 1)
	goto check_eof;
    /*
       Resize buffer to fit the stack on file before reading file
    */
    status = grow_bmstack(out->stack, out->len);
    if (status)
	return status;;
    if (out->len == 0)
	return E_FFORMAT;
    if (fread(out->stack, sizeof *out->stack, out->len, out->fh) != out->len)
	goto check_eof;

    return SUCCESS;
 check_eof:
    return feof(out->fh) ? E_FFORMAT : E_IO;
}

/* Saves the Bookmark structure to disk. 

   Should only be called if stack is not empty (out->n > 0)

   File format:    | n | len | stack |      */
static ErrCode
save_bmstack(struct Bookmarks *out)
{
    assert(out->n > 0 && "Won't write empty stack"); 

    if (fwrite(&out->n, sizeof out->n, 1, out->fh) != 1)
	goto err;
    if (fwrite(&out->len, sizeof out->len, 1, out->fh) != 1)
	goto err;
    if (fwrite(out->stack, sizeof *out->stack, out->len, out->fh) != out->len)
	goto err;

    return SUCCESS;
 err:
    return E_IO;
}

/* Resizes stack to fit 'newlen' entries. */
static ErrCode
grow_bmstack(long *stack, size_t newlen)
{
    stack = realloc(stack, newlen * (sizeof *stack));

#ifdef DEBUG
    printf("BMstack: resized to %uB\n", newlen);
#endif

    return stack == NULL ? E_MEM : SUCCESS;
}
