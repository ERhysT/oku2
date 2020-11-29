/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* err.h - Error code enumeration and printing error strings */

#ifndef OKU_ERR_H
#define OKU_ERR_H

typedef enum err_code
    {	SUCCESS,
	E_ARG,
	E_SPI,
	E_IO,
	E_GPIO,
	E_CLEANUP,
	E_BUSY,
	E_SIG,
	E_INIT,
	E_SLEEP,
	E_TERM
    } ErrCode;

/* Print error strings */
void err_print(ErrCode status);

#endif	/* OKU_ERR_H */
