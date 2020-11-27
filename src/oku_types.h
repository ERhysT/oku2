/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* oku_types.h - Data type definitions */

#ifndef OKU_TYPES_H
#define OKU_TYPES_H

#include <stdint.h>

/* Must be an eight bit byte */
typedef unsigned char byte;

/* Coordinate system */
typedef uint16_t coordinate;

typedef struct _point {
    coordinate x;		/* The abscissa */
    coordinate y;		/* The ordinate */
} Point ;

/* Rectangle defined by two verticies */
typedef struct _resolution {
    Point origin;
    Point extent;
} Rectangle ;






#endif	/* OKU_TYPES_H */
