/* This file is part of oku - an electronic paper book reader
   Copyright (C) 2020  Ellis Rhys Thomas <e.rhys.thomas@gmail.com>
   See COPYING for licence details. */

/* epd.c - EPD userspace driver: 3-wire spi communication
   implementation specific to Waveshare 2.9" Module with HAT. */

#include <unistd.h>
#include <time.h>

#include "gpio.h"
#include "spi.h"
#include "oku_types.h"
#include "err.h"

#include "epd.h"

/* Interface connections pinmap (3.3/0V lines not included).

   | Pin  | Description           | BCM 2835 Pin | Pi Physical Pin |
   |------+-----------------------+--------------+-----------------|
   | DIN  | SPI MOSI              | 10           |              19 |
   | CLK  | SPI SCK               | 11           |              23 |
   | CS   | SPI chip select       | 8            |              24 |
   | DC   | Data/Command control  | 25           |              22 |
   | RST  | External reset pin    | 17           |              11 |
   | BUSY | Busy state output pin | 24           |              18 |  */

/* Device dimensions in pixels. The pitch for this device is
   horizontal i.e one byte represents 8 packed pixels across the
   width. A helper macro to calculate the pitch packing is
   provided. */
#define WIDTH		128	
#define HEIGHT		296	
#define PITCH(X)        ( X%8 ? 1+(X/8) : X/8 ) /* px packed to bytes */

/* Interface Configuration */
#define GPIO_DEVICE       "/dev/gpiochip0"
#define GPIO_CONSUMER     "oku"
#define SPI_DEVICE        "/dev/spidev0.0"
#define SPI_MODE          (0|0)	           /* CPOL=0, CPHA=0 */
#define SPI_CLKSPEED_MHZ  10	           /* 10 MHz */
#define LUT_LEN           30		   /* bytes in lut register */

/* Timings */
#define BUSY_DELAY        100 	           /* busy read sample delay (ms) */
#define GPIO_DELAY        200 	           /* sample delay time (ms) */
#define REFRESH_DELAY     500		   /* wait after refreshing display */

/* pin numbers use BCM2835 numbering not pi physical numbers. DIN
   (MOSI) and CLK are not enumerated as they are not manually
   managed with gpio.h. */
enum BCM_PIN	     
    {
	BCM_PIN_ChipSelect  =  8, /* pi->epd: Low when SPI active */
	BCM_PIN_Reset       = 17, /* pi->epd: Low to reset epd  */
	BCM_PIN_DataCommand = 25, /* pi->epd: High when sending command */
	BCM_PIN_Busy        = 24  /* epd->pi: Low when busy */
    };

/* Internal representation of pixel colour for this device */
enum {BLACK = 0x00 , WHITE = 0xFF};

/* EPD command bytes */
enum COMMAND
    { DRIVER_OUTPUT_CONTROL                  = 0x01,
      BOOSTER_SOFT_START_CONTROL             = 0x0C,
      GATE_SCAN_START_POSITION               = 0x0F,
      DEEP_SLEEP_MODE                        = 0x10,
      DATA_ENTRY_MODE_SETTING                = 0x11,
      SW_RESET                               = 0x12,
      TEMPERATURE_SENSOR_CONTROL             = 0x1A,
      MASTER_ACTIVATION                      = 0x20,
      DISPLAY_UPDATE_CONTROL_1               = 0x21,
      DISPLAY_UPDATE_CONTROL_2               = 0x22,
      WRITE_RAM                              = 0x24,
      WRITE_VCOM_REGISTER                    = 0x2C,
      WRITE_LUT_REGISTER                     = 0x32,
      SET_DUMMY_LINE_PERIOD                  = 0x3A,
      SET_GATE_TIME                          = 0x3B,
      BORDER_WAVEFORM_CONTROL                = 0x3C,
      SET_RAM_X_ADDRESS_START_END_POSITION   = 0x44,
      SET_RAM_Y_ADDRESS_START_END_POSITION   = 0x45,
      SET_RAM_X_ADDRESS_COUNTER              = 0x4E,
      SET_RAM_Y_ADDRESS_COUNTER              = 0x4F,
      TERMINATE_FRAME_READ_WRITE             = 0xFF  };

/* Data corresponding to command bytes enumeration */
const byte driver_output_control[] =      { (HEIGHT-1) &0xFF, ((HEIGHT-1)>>8) &0xFF, 0}; /* GD=0, SM=0, TB=0 */
const byte booster_soft_start_control[] = { 0xD7, 0xD6, 0x9D };
const byte write_vcom_register[] =        { 0xA8 }; /* Vcom=7C */
const byte set_dummy_line_period[] =      { 0x1A }; /* 4 lines/gate */
const byte set_gate_time[] =              { 0x08 }; /* 2us/line */
const byte border_waveform_control[] =    { 0x03 };
const byte data_entry_mode_setting[] =    { 0x03 };
const byte write_lut_register[] =         { 0x32 };
const byte display_update_control_2[] =   { 0xC4 };
const byte deep_sleep_mode[] =            { 0x01 };

/* look up table registers */
const byte lut_full_update[LUT_LEN] = {
    0x50, 0xAA, 0x55, 0xAA, 0x11, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0x1F, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const byte lut_partial_update[LUT_LEN] = {
    0x10, 0x18, 0x18, 0x08, 0x18, 0x18,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x13, 0x14, 0x44, 0x12,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* FORWARD DECLARATIONS */
static ErrCode init_gpio(void);
static ErrCode init_spi(void);

static ErrCode dev_reset(void); 
static ErrCode dev_init(void);
static ErrCode dev_set_ram_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
static ErrCode dev_set_ram_cursor(uint16_t x, uint16_t y);
static ErrCode dev_wait_while_busy(void);
static ErrCode dev_poweroff(void);

static ErrCode transmit_command(const byte tx);
static ErrCode transmit_data(const byte *tx, size_t len);
static ErrCode transmit_lut(const byte *lut);

static ErrCode delay(unsigned ms);

/* INTERFACE IMPLEMENTATION */

/* Start communications and transmit initialisation sequences to EPD. */
ErrCode
epd_start(void)
{
    ErrCode status;

    status = init_gpio();
    if (status)
	goto err;

    status = init_spi();
    if (status)
	goto err;

    status = dev_reset();
    if (status)
	goto err;

    status = dev_init();
    if (status)
	goto err;

    status = transmit_lut(lut_full_update);
    if (status)
	goto err;

 err:
    return status;
}

/* Sets every bit in EPD RAM representing a pixel to the defined value
   of WHITE within the provided rectangle.

   For this device, the cursor needs to be reset set for each row. The
   data representing the pitch can then be transmitted byte by byte. */
ErrCode
epd_clear(void)
{
    ErrCode status;
    const byte colour[] = { WHITE };

    /* px coordiantes into packed bytes */
    uint16_t x = 0;		/* initial byte */
    uint16_t y = 0;
    uint16_t X = PITCH(WIDTH);	/* final byte */
    uint16_t Y = HEIGHT;

    status = dev_set_ram_window(x, y, WIDTH, HEIGHT);
    if (status)
	goto err;

    /* It is required to transmit pixel data row by row */
    for (y=0; y<Y; y++) {
	status = dev_set_ram_cursor(0, y);
	if ( status)
	    goto err; 

	status = transmit_command(WRITE_RAM);
	if (status)
	    goto err;
	
	for (x=0; x<X; x++) {
	    status = transmit_data(colour, sizeof colour);
	    if (status)
		goto err;
	}
    }
    
 err:
    return status;
}

ErrCode
epd_refresh(void)
{
    ErrCode status;

    status = transmit_command(DISPLAY_UPDATE_CONTROL_2);
    if (status)
	goto err;
    status = transmit_data(display_update_control_2, sizeof display_update_control_2);
    if (status)
	goto err;
    status = transmit_command(MASTER_ACTIVATION);
    if (status)
	goto err;
    status = transmit_command(TERMINATE_FRAME_READ_WRITE);
    if (status)
	goto err;

    status = dev_wait_while_busy();

    /* After device leaves busy, screen will be refreshed and device
       idle after 500s */
    delay(REFRESH_DELAY); 		

 err:
    return status;
}

ErrCode
epd_stop(void)
{
    ErrCode status;

    status = dev_poweroff();
    GPIO_stop();
    SPI_stop();

    return status;
}

/* STATIC FUNCTIONS */

/* Starts the gpio library, reserves pins, sets operating direction
   and initial values. */
static ErrCode
init_gpio(void)
{ 
    ErrCode status;

    status = GPIO_start(GPIO_DEVICE, GPIO_CONSUMER);
    if (status)
	goto err; 

    status = GPIO_reserve_input(BCM_PIN_Busy);
    if (status)
	goto err;
    status = GPIO_reserve_output(BCM_PIN_ChipSelect, GPIO_LEVEL_High);
    if (status)
	goto err;
    status = GPIO_reserve_output(BCM_PIN_Reset, GPIO_LEVEL_High);
    if (status)
	goto err;
    status = GPIO_reserve_output(BCM_PIN_DataCommand, GPIO_LEVEL_High);
    if (status)
	goto err;
    
    /* there is a problem with cs changing itself, maybe setting
       manually here will help. */
    status = GPIO_write(BCM_PIN_ChipSelect, GPIO_LEVEL_High);
    if (status)
	goto err;

 err:
    return status;
}

static ErrCode
init_spi(void)
{
    return SPI_start(SPI_DEVICE, SPI_CLKSPEED_MHZ);
}

/* Software reset of epd by holding reset pin low, even on error pins
   always returned to original state. */
static ErrCode
dev_reset(void)
{
    ErrCode status, status_cleanup;

    status = GPIO_write(BCM_PIN_Reset, GPIO_LEVEL_Low);
    if (status) 
	goto err;

    status = delay(GPIO_DELAY);
    if (status) 
	goto err;
    
    status = GPIO_write(BCM_PIN_Reset, GPIO_LEVEL_Low);
    if (status) 
	goto err;

    status = delay(GPIO_DELAY);
    if (status) 
	goto err;

 err:
    status_cleanup = GPIO_write_default(BCM_PIN_Reset);

    status = delay(GPIO_DELAY);
    if (status) 
	goto err;

    return status ? status : status_cleanup;
}

/* Send initialisation sequence to epd */
static ErrCode
dev_init(void)
{
    ErrCode status;

    status = transmit_command(DRIVER_OUTPUT_CONTROL);
    if (status)
	goto err;
    status = transmit_data(driver_output_control, sizeof driver_output_control);
    if (status)
	goto err;

    status = transmit_command(BOOSTER_SOFT_START_CONTROL);
    if (status)
	goto err;
    status = transmit_data(booster_soft_start_control, sizeof booster_soft_start_control);
    if (status)
	goto err;

    status = transmit_command(WRITE_VCOM_REGISTER);
    if (status)
	goto err;
    status = transmit_data(write_vcom_register, sizeof write_vcom_register);
    if (status)
	goto err;

    status = transmit_command(SET_DUMMY_LINE_PERIOD);
    if (status)
	goto err;
    status = transmit_data(set_dummy_line_period, sizeof set_dummy_line_period);
    if (status)
	goto err;

    status = transmit_command(SET_GATE_TIME);
    if (status)
	goto err;
    status = transmit_data(set_gate_time, sizeof set_gate_time);
    if (status)
	goto err;

    status = transmit_command(BORDER_WAVEFORM_CONTROL);
    if (status)
	goto err;
    status = transmit_data(border_waveform_control, sizeof border_waveform_control);
    if (status)
	goto err;

    status = transmit_command(DATA_ENTRY_MODE_SETTING);
    if (status)
	goto err;
    status = transmit_data(data_entry_mode_setting, sizeof data_entry_mode_setting);
    if (status)
	goto err;

 err:
    return status;
}

/* Set the ram windows (arguements are using pixel coordinates byte
   offsets i.e. do NOT provide x packed with pitch) */
static ErrCode
dev_set_ram_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    ErrCode status;

    const byte ram_x_window[] = {
	(x0 >> 3) & 0xFF,	/* divide by 8 for pitch */
	(x1 >> 3) & 0xFF  };
    const byte ram_y_window[] = {
	y0        & 0xFF,	/* send in 2 bytes as HEIGHT > 255px */
	(y0 >> 8) & 0xFF,
	y1        & 0xFF,
	(y1 >> 8) & 0xFF  };	

    status = transmit_command(SET_RAM_X_ADDRESS_START_END_POSITION);
    if (status)
	goto err;
    status = transmit_data(ram_x_window, sizeof ram_x_window);
    if (status)
	goto err;
    status = transmit_command(SET_RAM_Y_ADDRESS_START_END_POSITION);
    if (status)
	goto err;
    status = transmit_data(ram_y_window, sizeof ram_y_window);
    if (status)
	goto err;

 err:
    return status;
}

static ErrCode
dev_set_ram_cursor(uint16_t x, uint16_t y)
{
    ErrCode status;

    const byte x_cur[] = { (x >> 3) & 0xFF }; /* packed 8bits per byte */
    const byte y_cur[] = { y & 0xFF, (y >> 8) & 0xFF }; /* can be >255 so 2B req. */

    status = transmit_command(SET_RAM_X_ADDRESS_COUNTER);
    if (status)
	goto err;
    status = transmit_data(x_cur, sizeof x_cur);
    if (status)
	goto err;

    status = transmit_command(SET_RAM_Y_ADDRESS_COUNTER);
    if (status)
	goto err;
    status = transmit_data(y_cur, sizeof y_cur);
    if (status)
	goto err;

 err:
    return status; 
}

/* Wait until busy line is low or tries expired. Returns E_BUSY if
   tries expired. */
static ErrCode
dev_wait_while_busy(void)
{
    ErrCode status;
    enum GPIO_LEVEL busy;
    unsigned tries = 300; 	/* ~30s if BUSY_DELAY is 100ms  */

    do {
	status = delay(BUSY_DELAY);
	if (status)
	    break;
	status = GPIO_read(BCM_PIN_Busy, &busy);
	if (status)
	    break;
    } while (busy==GPIO_LEVEL_High && tries--); 

    return tries ? status : E_BUSY;
}

/* Attempts to power off the device, failure can result in device
   damage. For this reason, E_SLEEP is returned here (and only here),
   overwriting subrotine statuses - and must be checked for by
   caller. */
static ErrCode
dev_poweroff(void)
{
    ErrCode status;

    status = transmit_command(DEEP_SLEEP_MODE);
    if (status)
	goto err;

    status = transmit_data(deep_sleep_mode, sizeof deep_sleep_mode);

 err:
    return status ? E_SLEEP : SUCCESS;
}

/* Transmits a command byte to the epd.

  For command transfer:
       DC pin low (command)
       CS pin low (epd selected)

  After spi transmission chip must be deselected (CS high) chip to
  complete transfer */
static ErrCode
transmit_command(const byte tx)
{
    ErrCode status;

    status = GPIO_write(BCM_PIN_DataCommand, GPIO_LEVEL_Low);
    if (status)
	goto err;
    status = GPIO_write(BCM_PIN_ChipSelect, GPIO_LEVEL_Low);
    if (status)
	goto err;

    GPIO_dump();
    status = SPI_write_byte(tx);
    if (status)
	goto err;

    status = GPIO_write(BCM_PIN_ChipSelect, GPIO_LEVEL_High);

 err:
    return status; 
}

/* Transmit data byte to epd

   For data transmission:
        DC pin high (data)
        CS pin low (epd selected)

  After spi transmission chip must be deselected (CS high) to complete
  transfer */
static ErrCode
transmit_data(const byte *tx, size_t len)
{
    ErrCode status;
    size_t i;

    status = GPIO_write(BCM_PIN_DataCommand, GPIO_LEVEL_High);
    if (status)
	goto err;

    for (i=0; i<len && status==SUCCESS; ++i) {   
	GPIO_write(BCM_PIN_ChipSelect, GPIO_LEVEL_Low);
	GPIO_dump();
	status = SPI_write_byte(tx[i]);
	GPIO_write(BCM_PIN_ChipSelect, GPIO_LEVEL_High);
    }

 err:
    return status;
}

static ErrCode
transmit_lut(const byte *lut)
{
    ErrCode status;

    status = transmit_command(WRITE_LUT_REGISTER);
    if (status)
	goto err;

    status = transmit_data(lut, LUT_LEN);
    if (status)
	goto err;

 err:
    return status;
}

/* sleeps process (milli seconds) */
static ErrCode
delay(unsigned ms)
{
    int res;
    unsigned i;

    if (ms == 0)
	return E_ARG;

    for (i=0, res=0; i<ms && !res; ++i) /* loop prevents integer overflow */
	res = usleep(1000);

    return res ? E_SIG : SUCCESS;
}
