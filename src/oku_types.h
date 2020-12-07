/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* oku_types.h - Data type definitions */

#ifndef OKU_TYPES_H
#define OKU_TYPES_H

#include <stdint.h>

/* Unicode codepoints */
#define CODEPOINT_INVALID_CHAR 0x0000FFFD

/* unicode codepoint (maximum 21bits U+10FFFF) */
typedef uint32_t unicode;

/* Must be an octet */
typedef unsigned char byte;

/* Coordinate system large enough to describe all pixel coordinates */
typedef uint16_t coordinate;

struct Point {
    coordinate     x;		/* The abscissa */
    coordinate     y;		/* The ordinate */
};

struct Raster {
    struct Point   size;	/* px from top left origin */
    byte          *bitmap; 
};

/* Bitmap data for one character glyph */
struct Glyph {
    unicode        codepoint;
    struct Raster  render;
};

/* Bookmark */
struct Bookmark {
    const char    *book_path;
    long           position;
}
    
#endif	/* OKU_TYPES_H */
