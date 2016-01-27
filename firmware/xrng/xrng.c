/*
 * xrng.c
 *
 * Created: 12/01/2016 14:44:47
 *  Author: paul.qureshi
 */ 


#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include "asf.h"

#include "global.h"
#include "hw_misc.h"
#include "usb.h"
#include "rng.h"
#include "debug.h"


#define VERSION_MAJOR	1
#define VERSION_MINOR	0

#pragma region firmware image identifier
typedef struct {
	char		magic_string[8];
	uint8_t		version_major;
	uint8_t		version_minor;
	uint8_t		mcu_signature[3];
	uint32_t	flash_size_b;
	uint16_t	page_size_b;
	uint32_t	eeprom_size_b;
	uint16_t	eeprom_page_size_b;
} FW_INFO_t;

// data embedded in firmware image so that the bootloader program can read it
volatile const __flash FW_INFO_t firmware_info =	{	{ 0x59, 0x61, 0x6d, 0x61, 0x4e, 0x65, 0x6b, 0x6f },		// "YamaNeko" magic identifier
														VERSION_MAJOR,
														VERSION_MINOR,
														{ SIGNATURE_0, SIGNATURE_1, SIGNATURE_2 },
														APP_SECTION_SIZE,
														APP_SECTION_PAGE_SIZE,
														EEPROM_SIZE,
														EEPROM_PAGE_SIZE
													};
#pragma endregion


COMPILER_WORD_ALIGNED uint8_t buffer_a[64];
COMPILER_WORD_ALIGNED uint8_t buffer_b[64];
uint8_t buffer_sel = 0;
volatile uint8_t transfer_complete = 0xFF;

/**************************************************************************************************
** Detect when the last transfer has completed (or been aborted/failed)
*/
void main_vendor_bulk_in_received(udd_ep_status_t status, iram_size_t nb_transfered, udd_ep_id_t ep)
{
	//transfer_complete = 0xFF;
	RND_get_buffer64(buffer_a);
	udi_vendor_bulk_in_run(buffer_a, 64, main_vendor_bulk_in_received);
}

/**************************************************************************************************
** Main entry point and main loop
*/
int main(void)
{
	firmware_info.magic_string[0];	// prevent firmware_info being optimized away

	sysclk_init();
	HW_init();
	USB_init();
	RNG_init();
	DBG_init();
	
	//PORTCFG.CLKEVOUT = PORTCFG_CLKOUTSEL_CLK1X_gc | PORTCFG_CLKOUT_PC7_gc;
	//PORTC.DIRSET = PIN7_bm;

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
	sei();
	udc_start();
	udc_attach();

	udi_vendor_bulk_in_run(buffer_a, 64, main_vendor_bulk_in_received);
	for(;;)

	for(;;)
	{
		uint8_t i;
		WDR();

		// use alternate buffers
		uint8_t *buf;
		if (buffer_sel)
			buf = buffer_b;
		else
			buf = buffer_a;
		buffer_sel = !buffer_sel;
		
		//for (uint8_t i = 0; i < 64; i++)
		//	buf[i] = rand();
		RND_get_buffer64(buf);
		//memset(buf, 0, 64);
		//buf[0] = 0;
		//buf[2] = 0;
		//*(volatile uint8_t *)buf = 0;

		for (i = 0; i < 100; i++)
		{
			if (transfer_complete)
				break;
			_delay_us(1);
		}
		if (i > 0x7F)
			continue;
		transfer_complete = 0;
/*		
		for (i = 0; i < 100; i++)
		{
			if (udi_vendor_bulk_in_run(buf, 64, main_vendor_bulk_in_received))
				break;
			_delay_ms(1);
		}
		transfer_complete = 0;
*/
		_delay_ms(1);
		udi_vendor_bulk_in_run(buf, 64, main_vendor_bulk_in_received);
	}
}