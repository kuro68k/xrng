/*
 * xrng.c
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "xmega/usb_xmega.h"
#include "rng.h"

#include "usb.h"
#include "xmega/usb_xmega_internal.h"
#define _USB_EP(epaddr) \
	USB_EP_pair_t* pair = &usb_xmega_endpoints[(epaddr & 0x3F)]; \
	USB_EP_t* e __attribute__ ((unused)) = &pair->ep[!!(epaddr&0x80)]; \


#define	EP_BULK_IN		0x81
#define EP_BULK_OUT		0x02
#define BUFFER_SIZE		960

int main(void)
{
	usb_configure_clock();

	PORTCFG.CLKEVOUT = PORTCFG_CLKOUTSEL_CLK1X_gc | PORTCFG_CLKOUT_PC7_gc;
	PORTC.DIRSET = PIN7_bm;
	//for(;;);

	RNG_init();

	// Enable USB interrupts
	USB.INTCTRLA = /*USB_SOFIE_bm |*/ USB_BUSEVIE_bm | USB_INTLVL_MED_gc;
	USB.INTCTRLB = USB_TRNIE_bm | USB_SETUPIE_bm;

	usb_init();

	USB.CTRLA |= USB_FIFOEN_bm;

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
	sei();

	usb_attach();
	usb_ep_enable(EP_BULK_IN, USB_EP_TYPE_BULK, 64);
	usb_ep_enable(0x01, 0, 64);

	uint8_t buffer_a[BUFFER_SIZE];
	uint8_t buffer_b[BUFFER_SIZE];
	_USB_EP(EP_BULK_IN);

	//USB_EP_pair_t* pair = &usb_xmega_endpoints[(0x81 & 0x3F)];
	//USB_EP_t* ea = &pair->in;
	//USB_EP_t* eb = &pair->out;
	//ea->CTRL |= USB_EP_PINGPONG_bm;

	e->STATUS |= USB_EP_TRNCOMPL0_bm;	// kickstart the loop
	for(;;)
	{
		//for (uint16_t i = 0; i < BUFFER_SIZE; i++)
		//	buffer_a[i] = i;
		RNG_get_buffer(buffer_a, BUFFER_SIZE);
		while((e->STATUS & USB_EP_TRNCOMPL0_bm) == 0);

		e->DATAPTR = (uint16_t)buffer_a;
		e->AUXDATA = 0;	// for multi-packet
		e->CNT = BUFFER_SIZE;// | USB_EP_ZLP_bm;
		LACR16(&(e->STATUS), USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm | USB_EP_OVF_bm);

		//for (uint16_t i = 0; i < BUFFER_SIZE; i++)
		//	buffer_b[i] = i;
		RNG_get_buffer(buffer_b, BUFFER_SIZE);
		while((e->STATUS & USB_EP_TRNCOMPL0_bm) == 0);

		e->DATAPTR = (uint16_t)buffer_b;
		e->AUXDATA = 0;	// for multi-packet
		e->CNT = BUFFER_SIZE;// | USB_EP_ZLP_bm;
		LACR16(&(e->STATUS), USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm | USB_EP_OVF_bm);
	}
}

		//while (!usb_ep_ready(EP_BULK_IN));
		//while (!usb_ep_pending(EP_BULK_IN));
		//usb_ep_start_in(EP_BULK_IN, buffer, BUFFER_SIZE, false);
