/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* gpio.c - GPIO interface implemented using gpiod.h    */

#ifdef DEBUG
#include <stdio.h>
#endif

#include <stddef.h>
#include <gpiod.h>

#include "oku_types.h"
#include "err.h"
#include "gpio.h"

#define LINE_MAX 50		/* maximum number of gpiolines in a chip */

struct line {
    struct gpiod_line *handle;	   /* libgpiod gpio line handle */
    enum GPIO_LEVEL default_level; /* default level  */
};

struct gpio {
    const char *device;	        /* char device string i.e. /dev/gpiodevX.X */
    const char *consumer;	/* identifying name for user */
    struct gpiod_chip *handle;	/* libgpiod gpio chip handle */
    struct line line[LINE_MAX];	/* array of lines on chip */
};

struct gpio gpio;

ErrCode
GPIO_start(const char *device, const char *consumer)
{
    if (gpio.handle)		/* instance already open */
	return E_INIT;
    if (!device || !consumer)	/* validate arguements */
	return E_ARG;

    gpio.device = device;
    gpio.consumer = consumer;

    gpio.handle = gpiod_chip_open(gpio.device);

    return gpio.handle ? SUCCESS : E_GPIO;
}

void
GPIO_stop(void)
{
    if (gpio.handle)  {
	gpiod_chip_close(gpio.handle);
	gpio.handle = NULL;
    }
    
    return;
}

ErrCode
GPIO_reserve_input(unsigned line)
{
    if (!gpio.handle || line >= LINE_MAX)
	return E_INIT;

    gpio.line[line].handle = gpiod_chip_get_line(gpio.handle, line);
    if (!gpio.line[line].handle)
	return E_IO;

    return gpiod_line_request_input(gpio.line[line].handle,
				    gpio.consumer) == -1
	? E_GPIO : SUCCESS;
}

/* Select the gpio pin from a line number and set the output to the
   default level */
ErrCode
GPIO_reserve_output(unsigned line, enum GPIO_LEVEL initial) 
{
    if (!gpio.handle || line >= LINE_MAX)
	return E_INIT;

    gpio.line[line].handle = gpiod_chip_get_line(gpio.handle, line);
    if (!gpio.line[line].handle)
	return E_IO;

    gpio.line[line].default_level = initial;

    return gpiod_line_request_output(gpio.line[line].handle,
				     gpio.consumer, 
				     gpio.line[line].default_level) == -1
	? E_GPIO : SUCCESS;
}

ErrCode
GPIO_write(unsigned line, enum GPIO_LEVEL in)
{
    if (!gpio.handle)
	return E_INIT;

    return gpiod_line_set_value(gpio.line[line].handle, in)
	? E_GPIO : SUCCESS;
}

ErrCode
GPIO_write_default(unsigned line)
{
    if (!gpio.handle)
	return E_INIT;

   return gpiod_line_set_value(gpio.line[line].handle,
			       gpio.line[line].default_level)
	? E_GPIO : SUCCESS;
}

ErrCode
GPIO_read(unsigned line, enum GPIO_LEVEL *out)
{
    if (!gpio.handle)
	return E_INIT;

    int res = gpiod_line_get_value(gpio.line[line].handle);
    if (res == -1)
	return E_GPIO;
    else
	*out = res;

    return SUCCESS;
}

void
GPIO_dump(void)
{
#ifdef DEBUG   
    printf("Active GPIO pins: ");
    for (int i=0; i<LINE_MAX; ++i)
	if (gpio.line[i].handle != NULL)
	    printf("%d[%d] ", i, gpiod_line_get_value(gpio.line[i].handle));

    printf("\n");
#endif

    return;
}
