/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* unifont.c - Reads fonts in the GNU Unifont .hex format.

  The format provides an ASCI encoded text file with one line per
  unicode codepoint.

  Each line starts with a 4 character hexadecimal representation of
  the codepoint followed by a delimiting colon. 

  Next is a hexadecimal representation of a glyph, the format defines
  its glyphs as either 8 or 16 pixels in width by 16 pixels in
  height. 

  Most Western script glyphs can be defined as 8 pixels wide, while
  other glyphs (Chinese, Japanese and Korean etc.) are often 16 pixels
  wide.

  Glyphs are packed horizontally with a pitch of 8, with the origin at
  the top left. The height always represents 16 bytes

  For example, representation of 'a' (8x16px) is shown below.

  0021:00000000080808080808080008080000

  FIELD                       N CHARACTERS
  unicode codepoint                      4
  delimiting ':'                         1
  bitmap                          32 or 64   
  newline (unless EOF)                   1
                         MAX TOTAL  =   70

  Minimum buffer size to hold any line is 70+'\0' = 71B
 */

#include <stdlib.h>
#include <stdio.h>

#include "err.h"
#include "oku_types.h"

#include "unifont.h"

#define LINEMAX   71		/* max length of line */

static ErrCode get_line(FILE *fhandle, char *buf, size_t len);
static ErrCode scan_line(char *toscan, unicode *cp_out, char *bmp_out);

ErrCode
unifont_open(const char *path_to_open, FILE **file_out)
{
    *file_out = fopen(path_to_open, "r");
    return *file_out ? SUCCESS : E_PATH;
}

void
unifont_close(FILE **font_to_close)
{
    if (!font_to_close || !*font_to_close)
	return; 

    fclose(*font_to_close);
    *font_to_close = NULL;

    return;
}

ErrCode
unifont_getglyph(FILE* fhandle, unicode codepoint)
{
    ErrCode       status;
    char          line_buf[LINEMAX], bmp[64+1];
    unicode       cp;

    do {
	status = get_line(fhandle, line_buf, sizeof line_buf);
	if (status)
	    goto err;
	status = scan_line(line_buf, &cp, bmp);
	if (status)
	    goto err;
    } while (cp!=codepoint);

#ifdef DEBUG
    printf("U+%04X Found!\nBitmap:%s\n", cp, bmp);
#endif

 err:
    rewind(fhandle);
    return status;
}

/* STATIC FUNCTIONS */

static ErrCode
get_line(FILE* fhandle, char *buf, size_t len)
{
    if (fgets(buf, len, fhandle))
	return SUCCESS;

    /* determine the type of failure */
    if (feof(fhandle)) { 
	clearerr(fhandle);
	return E_MISSINGCHAR;	/* codepoint not in file */
    }

    return E_IO;
}

static ErrCode
scan_line(char *toscan, unicode *cp_out, char *bmp_out)
{
    return sscanf(toscan, "%04X:%s", cp_out, bmp_out) == 2
	? SUCCESS : E_UNIFONT;
}
