/* Name: main.c
 * Project: HID-Test
 * Author: Christian Starkjohann
 * Creation Date: 2006-02-02
 * Tabsize: 4
 * Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt) or proprietary (CommercialLicense.txt)
 * This Revision: $Id: main.c 299 2007-03-29 17:07:19Z cs $
 */

#define F_CPU   12000000L    /* evaluation board runs on 4MHz */

#include <avr/io.h>
#include <avr/iom8.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "usbdrv/usbdrv.h"
#include "usbdrv/oddebug.h"
#include "ADC.c"

static const unsigned int HIGH_THRESHOLD=600;
static const unsigned int LOW_THRESHOLD=50;

static void hardwareInit(void)
{
uchar	i, j;
    PORTD = 0xfa;   /* 1111 1010 bin: activate pull-ups except on USB lines */
    DDRD = 0x07;    /* 0000 0111 bin: all pins input except USB (-> USB reset) */
	PORTC = 0xff;   /* activate all pull-ups */
    DDRC = 0;       /* all pins input */
	j = 0;
	while(--j){     /* USB Reset by device only required on Watchdog Reset */
		i = 0;
		while(--i); /* delay >10ms for USB reset */
	}
    DDRD = 0x02;    /* 0000 0010 bin: remove USB reset condition */
    /* configure timer 0 for a rate of 12M/(1024 * 256) = 45.78 Hz (~22ms) */
    TCCR0 = 5;      /* timer 0 prescaler: 1024 */
}

/* ------------------------------------------------------------------------- */

#define M1_1    (1<<0)
#define M1_2    (1<<1)
#define M2_1    (1<<2)
#define M2_2    (1<<3)
#define M3_1    (1<<4)
#define M3_2    (1<<5)
#define M4_1    (1<<6)
#define M4_2	(1<<7)
#define M5_1	(1<<0)
#define M5_2	(1<<1)
#define Light	(1<<2)

/* The following function returns an index for the first key pressed. It
 * returns 0 if no key is pressed.
 */

 static const uchar  keyReport[12][2] PROGMEM= {
/* none */  		   {0, 0},                     /* no key pressed */
/*  report bytes */    {M1_1, 0},
/*  report bytes */    {M1_2, 0},
/*  report bytes */    {M2_1, 0},
/*  report bytes */    {M2_2, 0},
/*  report bytes */    {M3_1, 0},
/*  report bytes */    {M3_2, 0},
/*  report bytes */    {M4_1, 0},
/*  report bytes */    {M4_2, 0},
/*  report bytes */    {0, M5_1},
/*  report bytes */    {0, M5_2},
/*  report bytes */    {0, Light},
};

static uchar keyPressed(void)
{
	unsigned int x=0;
	uchar j=0;

	for (uchar i=0; i<6; i++){
 		x=ADC_result(i);		
		if ((x<LOW_THRESHOLD))
			return (i+1)+j;
			if ((x<HIGH_THRESHOLD)){
				if (i==5) return (i+1)+j;
				else
				return (i+2)+j;
			}
		j++;
	}		
	return 0;
}
/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

static uchar    reportBuffer[2];    /* buffer for HID reports */
static uchar    idleRate;           /* in 4 ms units */

PROGMEM char usbHidReportDescriptor[27] = { /* USB report descriptor */
   	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
    0x29, 0x0b,                    //   USAGE_MAXIMUM (Button 11)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x0b,                    //   REPORT_COUNT (11)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
    0xc0                           // END_COLLECTION

};


static void buildReport(uchar key)
{
/* This (not so elegant) cast saves us 10 bytes of program memory */
    *(int *)reportBuffer = pgm_read_word(keyReport[key]);
}

uchar	usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    usbMsgPtr = reportBuffer;
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            buildReport(keyPressed());
            return sizeof(reportBuffer);
        }else if(rq->bRequest == USBRQ_HID_GET_IDLE){
            usbMsgPtr = &idleRate;
            return 1;
        }else if(rq->bRequest == USBRQ_HID_SET_IDLE){
            idleRate = rq->wValue.bytes[1];
        }
    }else{
        /* no vendor specific requests implemented */
    }
	return 0;
}

/* ------------------------------------------------------------------------- */

int	main(void)
{
uchar   key, lastKey = 0, keyDidChange = 0;
uchar   idleCounter = 0;

	wdt_enable(WDTO_2S);
    hardwareInit();
	ADC_init();
	odDebugInit();
	usbInit();
	sei();
    DBG1(0x00, 0, 0);
	for(;;){	/* main event loop */
		wdt_reset();
		usbPoll();
        key = keyPressed();
        if(lastKey != key){
            lastKey = key;
            keyDidChange = 1;
        }
        if(TIFR & (1<<TOV0)){   /* 22 ms timer */
            TIFR = 1<<TOV0;
            if(idleRate != 0){
                if(idleCounter > 4){
                    idleCounter -= 5;   /* 22 ms in units of 4 ms */
                }else{
                    idleCounter = idleRate;
                    keyDidChange = 1;
                }
            }
        }
        if(keyDidChange && usbInterruptIsReady()){
            keyDidChange = 0;
            /* use last key and not current key status in order to avoid lost
               changes in key status. */
            buildReport(lastKey);
            usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
        }
	}
	return 0;
}


/* ------------------------------------------------------------------------- */

