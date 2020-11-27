/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

#include <stdio.h>

#include "epd.h"

#include "oku_types.h"
#include "err.h"

void
start_signal_handler(void)
{
    return;
}

int
main(int argc, char *argv[])
{
    ErrCode status;

    if (argc != 1) {
	fprintf(stderr, "USAGE: %s", argv[0]);
	return E_ARG;
    }
	
    start_signal_handler();
	
    status = epd_start();
    if (status)
	goto err;

    status = epd_clear();
    if (status)
	goto err;

    status = epd_refresh();
    if (status)
	goto err;

err:
    err_print(status);
    err_print(epd_stop());

    return status;
}
