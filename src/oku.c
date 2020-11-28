/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "epd.h"

#include "oku_types.h"
#include "err.h"

volatile sig_atomic_t sigint;

void
handle_sigint(int signum)
{
    sigint = signum;
}

ErrCode
ignore_sigint(struct sigaction *h)
{
    h->sa_handler = handle_sigint;
    h->sa_flags   = 0;

    return sigaction(SIGINT, h, NULL) < 0
	? E_SIG : SUCCESS; 
}

int
main(int argc, char *argv[])
{
    ErrCode             status;
    struct sigaction    sigint_action;

    if (argc != 1) {
	fprintf(stderr, "USAGE: %s", argv[0]);
	goto err;
    }
	
    status = ignore_sigint(&sigint_action);
    if (status)
	goto err;

    status = epd_start();
    if (status)
	goto err;

    while (!sigint) {

	status = epd_clear();
	if (status)
	    break;

	status = epd_refresh();
	if (status)
	    break;

	sleep(5);
    }

    /* event loop broken, exit cleanly */

 err:
    err_print(status);
    err_print(epd_stop());

    return status;
}
