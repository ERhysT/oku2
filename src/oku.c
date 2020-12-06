/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>

#include "epd.h"
#include "book.h"
#include "unifont.h"

#include "oku_types.h"
#include "err.h"

#define DEFAULT_BOOK       "./book.utf8"
#define DEFAULT_FONT       "./unifont.hex"

volatile sig_atomic_t sigint;

void
handle_sigint(int signum)
{
    sigint = signum;
}

ErrCode
catch_sigint(struct sigaction *h)
{
    h->sa_handler = handle_sigint;
    h->sa_flags   = 0;
    if (sigemptyset(&h->sa_mask) < 0)
	return E_SIG;

    return sigaction(SIGINT, h, NULL) < 0
	? E_SIG : SUCCESS; 
}

/* disables input stream buffering allowing characters to be read as
   they are typed */
ErrCode
set_input_mode(struct termios *old_tattr)
{
    struct termios new_tattr;

    if (!isatty(STDIN_FILENO)                   ||
	tcgetattr(STDIN_FILENO, old_tattr)  < 0 ||
	tcgetattr(STDIN_FILENO, &new_tattr) < 0 ) 
	return E_TERM;

    new_tattr.c_lflag       &= ~(ICANON|ECHO);
    new_tattr.c_cc[VMIN]     = 1;
    new_tattr.c_cc[VTIME]    = 0;

    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_tattr) < 0
	? E_TERM : SUCCESS;
}

void
reset_input_mode(const struct termios *old_attr)
{
    tcsetattr(STDIN_FILENO, TCSANOW, old_attr);
}

int
main(int argc, char *argv[])
{
    ErrCode             status;
    struct sigaction    sigint_action;
    struct termios      old_tattr;

    const char         *book_path, *font_path;
    FILE               *book,      *font;

    struct Point        pen, paper; /* cursor and limits  */
    struct Glyph        glyph;	    /* to contain a rendered codepoint */

    switch (argc) {
    case  1:  book_path = DEFAULT_BOOK;          break;
    case  2:  book_path = argv[1];               break;
    default:  puts("USAGE: oku [filename]");     return E_ARG;
    }
    
    font_path = DEFAULT_FONT;

    status = set_input_mode(&old_tattr);
    if (status)	goto os_cleanup;
    status = catch_sigint(&sigint_action);
    if (status)	goto os_cleanup;

    status = book_open(book_path, &book); 
    if (status)	goto os_cleanup;
    status = unifont_open(font_path, &font);
    if (status)	goto os_cleanup;

    status = epd_start(&paper);  /* device powered: must be shutdown later */
    if (status)	goto os_cleanup;
    status = epd_clear();
    if (status)	goto epd_shutdown;
    status = epd_refresh();
    if (status)	goto epd_shutdown;

    pen.x = pen.y = 0; 		/* start writing at (top left) origin */

    do {
	/*
	   User Input
	*/
	fputs("Input: next(k) previous(j) quit(q)... ", stdout);
	fflush(stdout);
	switch (getchar()) {
	case 'j': puts("Not implemented.");          continue;
	case 'k': puts("Moving forward one page.");  break;
	case 'q': puts("Powering down device.");     goto epd_shutdown;
	case EOF: status = E_IO;                     goto epd_shutdown;
	default:  puts("Unrecognised character.");   continue;
	}

	while(!sigint) {
	    /*
	      Load bitmap of the next character
	    */
	    status = book_getchar(book, &glyph.codepoint);
	    if (status)	goto epd_shutdown;
	    status = unifont_render(font, &glyph);
	    if (status)	goto epd_shutdown;
	    /*
	      Check pen position before writing
	    */
	    if (pen.x+glyph.render.size.x > paper.x) {
		pen.y += glyph.render.size.y;
		pen.x  = 0;
	    }
	    if (pen.y+glyph.render.size.y > paper.y) {
		free(glyph.render.bitmap); 
		//status = book_ungetchar(book, glyph.codepoint);
		break;
	    }

#ifdef DEBUG
	    printf("Pen: (%03u,%03u) "
		   "Paper: (%03u,%03u) "
		   "Glyph: (%03u,%03u)\n",
		   pen.x, pen.y, paper.x, paper.y,
		   glyph.render.size.x, glyph.render.size.y);
#endif

	    status = epd_write(&glyph.render, pen);
	    if (status)	goto epd_shutdown;
	    /* 
	       Increment Pen
	    */
	    pen.x += glyph.render.size.x;
	    free(glyph.render.bitmap);
	}
	/* 
	   Update display
	*/
	status = epd_refresh();
	if (status) goto epd_shutdown;

    } while (!sigint);

    /* event loop broken, exit cleanly */

 epd_shutdown:
    err_print(epd_stop());
 os_cleanup:
    unifont_close(&font);
    book_close(&book);
    
    
    reset_input_mode(&old_tattr);

    err_print(status);

    return status;
}
