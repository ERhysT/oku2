/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* spi.c - SPI communication using spidev.h */

#ifdef DEBUG
#include <stdio.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <string.h>		/* for memset() */

#include "oku_types.h"
#include "err.h"
#include "spi.h"

/* Default clocking modes and configuration used unless overridden
   using SPI_set_X() see linux/spi/spidev.h */
#define CLOCK_MODE        (SPI_MODE_0|SPI_NO_CS)
#define BITS_PER_WORD     8
#define DELAY_US          0
#define CS_CHANGE         0

static void spi_dump(byte tx);

struct spi_defaults {
    const char *device;		  /* /dev/spiX.X */
    int fd;			  /* device file descriptor */
    uint8_t mode;		  /* bitfield */
    uint8_t bits_per_word;	  
    uint32_t speed_hz;		  /* target clock speed in Hz */
};

struct spi_defaults spi;

ErrCode
SPI_start(const char *device, uint64_t speed_mhz)
{
    if (spi.device)
	return E_INIT;

    spi.mode          = CLOCK_MODE;
    spi.device        = device;
    spi.speed_hz      = speed_mhz * 1000000;
    spi.bits_per_word = BITS_PER_WORD;
    spi.fd            = open(spi.device, O_RDWR);
    if (spi.fd < 0)
	return E_IO;

    if (ioctl(spi.fd, SPI_IOC_WR_MODE, &spi.mode)                   == -1 ||
	ioctl(spi.fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi.speed_hz)       == -1 ||
	ioctl(spi.fd, SPI_IOC_WR_BITS_PER_WORD, &spi.bits_per_word) == -1 ) 
	return E_SPI;

    return SUCCESS;
}

void
SPI_stop(void)
{
    if (!spi.device || spi.fd == -1)
	return; 

    close(spi.fd);
    spi.fd = -1;
    spi.device = NULL;

    return;
}

/* transmit a single byte without recieving response */
ErrCode
SPI_write_byte(byte tx)
{
    if (!spi.device)
	return E_INIT;

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof tr);

    tr.tx_buf        = (unsigned long)&tx;
    tr.len           = 1;

    spi_dump(tx);

    return ioctl(spi.fd, SPI_IOC_MESSAGE(1), &tr) < 1
	? E_SPI : SUCCESS;
}
	
/* Reads default device settings and outputs to stdout */
static void
spi_dump(byte tx)
{
#ifdef DEBUG
    static unsigned long count;
    uint8_t  lsb, bits, mode;
    uint32_t speed;

    ioctl(spi.fd, SPI_IOC_RD_MODE, &mode);
    ioctl(spi.fd, SPI_IOC_RD_LSB_FIRST, &lsb);
    ioctl(spi.fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    ioctl(spi.fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);    

    printf("SPI %05ld:0x%02hhx @%dMHz "
	   "MODE:0x%02hhx LSB:0x%02hhx BITS/WORD:%hhd\n",
	   ++count, tx, speed/1000000, mode, lsb, bits);
#endif
    (void)tx;			/* supress unused warning */

    return;
}

