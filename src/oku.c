/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "oku.h"
#include "err.h"

#include "epd.h"
#include "book.h"
#include "unifont.h"

#define DEFAULT_BOOK       "book.utf8"
#define DEFAULT_FONT       "unifont.hex"

/*
  Powers down device safely on error (see err.h). 
*/
#define ERR_CHECK(S) do { if(S) die(S); } while(0)
/*
  Forward Declarations
*/
ErrCode   catch_sig(struct sigaction *h);
void      handle_sig(int signum);
void      die(ErrCode status);
ErrCode   page_fward(void);
ErrCode   page_bward(void);
void      pen_print(void);
/*
  Signal handler is event loop condition
*/
volatile sig_atomic_t sig;
/*
  Interface objects
*/
struct Book         book;	    /* text file */
struct Point        pen, paper;     /* epd cursor, limits  */
struct Unifont      font;	    /* unifont file and cache */
struct Glyph        glyph;	    /* single rendered character */
struct Bookmarks    pages;	    /* file position log */

/* Callback when SIGINT received, sigint  */
void
handle_sig(int signum)
{
    sig = signum;
}

/* Blocks SIGINT to allow safe powerdown of device */
ErrCode
catch_sig(struct sigaction *h)
{
    h->sa_handler = handle_sig;
    h->sa_flags   = 0;
    if (sigemptyset(&h->sa_mask) < 0)
	return E_SIG;

    return sigaction(SIGINT, h, NULL) < 0
	? E_SIG : SUCCESS; 
}

/* Save state and power down the device */
void
die(ErrCode status)
{
    err_print(epd_stop());

    bookmarks_close(&pages);
    unifont_close(&font);
    book_close(&book);

    err_print(status);
    exit(status);
}

/* Display next page on epd  */
ErrCode
page_fward(void)
{
    puts("\nMoving forward one page");
    while(!sig) {
	/*
	  Load the bitmap of the next character in the book
	*/
	ERR_CHECK( book_get_codepoint(&book, &glyph.codepoint));
	ERR_CHECK( unifont_render(&font, &glyph));
	/*
	  Check space for glyph before writing
	*/
	if (pen.x+glyph.render.size.x > paper.x) { /* newline */
	    pen.y += glyph.render.size.y;
	    pen.x  = 0;
	}
	if (pen.y+glyph.render.size.y > paper.y) { /* page full */
	    free(glyph.render.bitmap); 
	    ERR_CHECK( book_unget_codepoint(&book, glyph.codepoint));
	    break;
	}
	pen_print();
	/*
	    Write glyph to epd buffer
	*/
	ERR_CHECK( epd_write(&glyph.render, pen));
	/* 
	   Increment pen after saving page bookmark 
	*/
	ERR_CHECK( bookmarks_push(&book, &pages));
	pen.x += glyph.render.size.x;
	free(glyph.render.bitmap);
    }

    return SUCCESS;
}

/* Display previous page on epd  */
ErrCode
page_bward(void)
{
    puts("\nMoving backwards one page");
    die(E_TODO);
    
    return E_UNREACHABLE;
}

/* Prints epd cursor positions */
void
pen_print(void)
{
#ifdef DEBUG
    printf("Pen: (%03u,%03u) "
	   "Paper: (%03u,%03u) "
	   "Glyph: (%03u,%03u)\n",
	   pen.x, pen.y, paper.x, paper.y,
	   glyph.render.size.x, glyph.render.size.y);
#endif
    return;
}

int
main(int argc, char *argv[])
{
    struct sigaction    sigint_action; /* signal handler */
    const char         *font_path, *book_path;

    setbuf(stdout, NULL);	/* disable buffering */

    switch (argc) {
    case  1:  book_path = DEFAULT_BOOK;          break;
    case  2:  book_path = argv[1];               break;
    default:  puts("USAGE: oku [filename]");     return E_ARG;
    }
    
    font_path  = DEFAULT_FONT;

    ERR_CHECK( catch_sig(&sigint_action));
    ERR_CHECK( book_open(book_path, &book)); 
    ERR_CHECK( unifont_open(font_path, &font));
    ERR_CHECK( bookmarks_open(&book, &pages));

    ERR_CHECK( epd_start(&paper));
    ERR_CHECK( epd_clear());

    pen.x = pen.y = 0; 		/* start writing at (top left) origin */

    while (!sig) {
	fputs("Input: next(k) previous(j) quit(q) then ^D... ", stdout);

	switch (getchar()) {
	case 'j': ERR_CHECK( page_bward());             break;
	case 'k': ERR_CHECK( page_fward());             break;
	case 'q': die(SUCCESS);                         break;
	default:  puts("Unrecognised character.");      continue;
	}

	ERR_CHECK(epd_refresh()); /* updates epd */
    } 

    die(SUCCESS);
    return E_UNREACHABLE;
}
