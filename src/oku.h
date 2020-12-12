/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* oku.h - Data type definitions for oku */

#ifndef OKU_H
#define OKU_H

#include <stdint.h>

/* Useful unicode codepoints */
#define CODEPOINT_INVALID_CHAR  0x0000FFFD

typedef uint32_t      unicode;	  /* codepoint (max 21bits) */
typedef unsigned char byte;	  /* octet */
typedef uint16_t      coordinate; /* epd pixel coordinate */
typedef uint16_t      checksum;	  /* file hash */

struct Point {
    coordinate        x;		/* The abscissa */
    coordinate        y;		/* The ordinate */
};

struct Raster {
    struct Point      size;	/* px from top left origin */
    byte             *bitmap; 	/* horizontally packed map */
};

/* Bitmap data for one character glyph */
struct Glyph {
    unicode           codepoint;
    struct Raster     render;
};

struct Book {
    checksum          fhash;	/* book file hash */
    size_t            len;	/* file length in bytes  */
    FILE             *fh; 	/* file handle */
};

struct Bookmarks {
    char             *fname;	/* filename generated from hash */
    FILE             *fh;	/* stack file handle */

    /* The following fields are recorded in fh above  */
    size_t            n, len;	/* entries and buffer length B  */
    long             *stack;	/* file position records */
};

#endif	/* OKU_TYPES_H */
