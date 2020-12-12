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
  newline \n or \r\n               1 or  2    * unless EOF
                         MAX TOTAL  =   71

  The buffer size required to hold any line in a unicode and a null
  terminating byte is therefore 72B.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "err.h"
#include "oku.h"

#include "unifont.h"

#define LINEMAX         71	/* max characters in a uhex line */
#define DELIMITER       ':'

ErrCode
unifont_open(const char *path_to_open, struct Unifont *new)
{
    new->fh = fopen(path_to_open, "r");
    return new->fh ? SUCCESS : E_PATH;
}

void
unifont_close(struct Unifont *toclose)
{
    if (toclose->fh) {
	fclose(toclose->fh);
	toclose->fh = NULL;
    }
}

/* Populates the Raster for a Glyph. Retrieves bitmap comlimentary to
   the codepoint defined in the Glyph structure from a GNU Unicode
   .hex file. */
ErrCode
unifont_render(struct Unifont *font, struct Glyph *out)
{
    ErrCode       status;
    char          line[LINEMAX+1], *line_cur;
    byte         *bmp_cur; 
    size_t        bmp_len;

    rewind(font->fh);

    /* Find the uhex line with the matching codepoint */
    do {
	if (fgets(line, LINEMAX+1, font->fh) == NULL) {
	    if (feof(font->fh)) { /* determine input error type */
		status = E_MISSINGCHAR;
	    } else {
		status = E_IO;
	    }

	    rewind(font->fh);
	    return status;
	}

    } while (out->codepoint != strtoul(line, &line_cur, 16));

#ifdef DEBUG
    printf("Unifont: 0x%04x: %s", out->codepoint, line);
#endif

    /* Check and move past the delimiter */
    if (*line_cur++ != DELIMITER)
	return E_FFORMAT;	/* invalid file format */

    out->render.bitmap = calloc(32, sizeof *out->render.bitmap); 
    if (out->render.bitmap == NULL)
	return E_MEM;

    bmp_len = 0;
    bmp_cur = out->render.bitmap;
    while (*line_cur!='\0' && !isspace(*line_cur)) {

	if (sscanf(line_cur, "%02hhX", bmp_cur) != 1)
	    return E_FFORMAT;

	line_cur += 2;		/* 2 hexadecimal characters for one byte */
	++bmp_cur;
	++bmp_len;
    }
	
    /* Record bitmap dimensions in pixels */
    switch (bmp_len) {
    case 16:
	out->render.size.x = 1 * 8;
	out->render.size.y = 16;
	/* could realloc here to save space */
	break;
    case 32:
	out->render.size.x = 2 * 8;
	out->render.size.y = 16;
	break;
    default:
	free(out->render.bitmap);
	out->render.bitmap = NULL;
	rewind(font->fh);
	return E_FFORMAT;
    }

#ifdef DEBUG
    printf("Bitmap: 0x");
    for (unsigned i=0; i<bmp_len; ++i)
	printf("%02hhx", out->render.bitmap[i]);
    printf("\n");
#endif

    rewind(font->fh);
    return SUCCESS;
}
