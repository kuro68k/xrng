/*
 * hw_misc.c
 *
 * Created: 18/12/2014 21:22:20
 *  Author: Paul Qureshi
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "global.h"
#include "hw_misc.h"

FUSES = {
	0xFF,		// fusebyte 0
	0x00,		// fusebyte 1
	0xBE,		// fusebyte 2
	0xFF,		// fusebyte	3
	0xF3,		// fusebyte 4
	0xE0,		// fusebyte 5
};


uint8_t	HW_last_reset_status;


/**************************************************************************************************
** Set up hardware after reset
*/
void HW_init(void)
{
	HW_last_reset_status = RST.STATUS;
	RST.STATUS = 0xFF;		// clear all reset flags

	// start watchdog
	WDR();
	while (WDT.STATUS & WDT_SYNCBUSY_bm);
	WDR();
	HW_CCPWrite(&WDT_CTRL, WDT_PER_128CLK_gc | WDT_ENABLE_bm | WDT_CEN_bm);

	SLEEP.CTRL = SLEEP_SMODE_IDLE_gc | SLEEP_SEN_bm;
}

/**************************************************************************************************
** Write a CCP protected register. Registers protected by CCP require the CCP register to be written
** first, followed by writing the protected register within 4 instruction cycles.
**
** Interrupts are temporarily disabled during the write. Code mostly adapted/stolen from Atmel's
** clksys_driver.c and avr_compiler.h.
*/
void HW_CCPWrite(volatile uint8_t *address, uint8_t value)
{
	uint8_t	saved_sreg;

	// disable interrupts if running
	saved_sreg = SREG;
	cli();
	
	volatile uint8_t * tmpAddr = address;
	RAMPZ = 0;

	asm volatile(
	"movw r30,  %0"       "\n\t"
	"ldi  r16,  %2"       "\n\t"
	"out   %3, r16"       "\n\t"
	"st     Z,  %1"       "\n\t"
	:
	: "r" (tmpAddr), "r" (value), "M" (CCP_IOREG_gc), "i" (&CCP)
	: "r16", "r30", "r31"
	);

	SREG = saved_sreg;
}
