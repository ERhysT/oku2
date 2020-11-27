/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* err.h - Error code enumeration and strings */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "err.h"

/* Error strings aligned with ErrorCode enumeration in err.h */
static const char *errcode_str[] =
    { "Success",
      "Invalid arguments",
      "SPI I/O failure",
      "Linux I/O failure",
      "GPIO failure",
      "Failed to clean up all resources",
      "Device busy",
      "Interupt signal received",
      "Device or resource uninitialised",
      "CRITICAL: sleep failed - remove power supply!"
    };

/* Prints error strings to stdout */
void
err_print(ErrCode status)
{
    if (status)
	fprintf(stderr, "[ERROR %d]\t%s\n", status, errcode_str[status]);
    if (errno)
	fprintf(stderr, "[ERRNO %d]\t%s\n", errno, strerror(errno));

    errno = 0;			/* reset errno */

    return;
}
