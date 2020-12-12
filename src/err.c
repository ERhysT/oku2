/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* err.h - Error code enumeration and strings */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "err.h"

/* Error strings aligned with ErrorCode enumeration in err.h */
static const char *errcode_str[] =
    { "Success",
      "Feature not implemented",
      "Invalid arguments",
      "SPI I/O failure",
      "Linux I/O failure",
      "GPIO failure",
      "Failed to clean up all resources",
      "Device busy",
      "Failed to initialise signal handling",
      "Device or resource uninitialised",
      "CRITICAL: sleep failed - remove power supply",
      "Not a terminal or misconfigured terminal",
      "Invalid file name",
      "Invalid UTF-8 file",
      "End of file",
      "Memory error",
      "Unicode codepoint undefined in font",
      "Invalid file format",
      "File is empty",
      "Corrupt .oku file (manually delete)" 
      "Buffer overflow prevented or detected"
    };

/* Prints error strings to stderr. */
void
err_print(ErrCode status)
{
    if (status)
	fprintf(stderr, "[ERROR %d]\t%s\n", status, errcode_str[status]);
    if (errno)
	fprintf(stderr, "[ERRNO %d]\t%s\n", errno, strerror(errno));

    err_clear_errno();

    return;
}

/* clear errno */
void
err_clear_errno(void)
{
    errno = 0;			/* reset errno */
}

/* terminates program with message to stderr if false */
void
assert_ptr(int exit_if_false)
{
    assert(exit_if_false && "Attemped to dereference a null pointer");
}
