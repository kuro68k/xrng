/*
 * rng.c
 *
 * Created: 12/01/2016 16:27:31
 *  Author: paul.qureshi
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <string.h>
#include "global.h"
#include "debug.h"
#include "hw_misc.h"
#include "rng.h"

volatile uint8_t	entropy[256];
volatile uint8_t	ent_write_head = 0;
volatile uint8_t	ent_read_head = 0;
volatile uint8_t	ent_bytes = 0;


/**************************************************************************************************
** Set up RNG
*/
void RNG_init(void)
{
	AES.CTRL = AES_RESET_bm;
	NOP();
	AES.CTRL = AES_XOR_bm;

	// fast/slow out of sync counters	
	CLK.RTCCTRL = CLK_RTCSRC_ULP_gc | CLK_RTCEN_bm;		// slow, jittery 1024Hz
	//CLK.RTCCTRL = (0b100 << CLK_RTCSRC_gp) | CLK_RTCEN_bm;	// slow, jittery 32768Hz
	RTC.CTRL = 0;
	while (RTC.STATUS & RTC_SYNCBUSY_bm);
	RTC.PER = 1;
	RTC.CNT = 0;
	RTC.INTCTRL = RTC_OVFINTLVL_LO_gc;
	RTC.CTRL = RTC_PRESCALER_DIV1_gc;
	
	FAST_TC.CTRLA = 0;
	FAST_TC.CTRLB = 0;
	FAST_TC.CTRLC = 0;
	FAST_TC.CTRLD = 0;
	FAST_TC.CTRLE = 0;
	FAST_TC.PER = 0xFFFF;
	FAST_TC.CTRLA = TC_CLKSEL_DIV1_gc;					// fast, crystal disciplined F_CPU

	// ADCA
	ADCA.CTRLA = ADC_ENABLE_bm;
	ADCA.CTRLB = ADC_FREERUN_bm | ADC_RESOLUTION_12BIT_gc;
	ADCA.REFCTRL = ADC_REFSEL_INTVCC_gc | ADC_TEMPREF_bm;
	ADCA.EVCTRL = 0;
	ADCA.PRESCALER = ADC_PRESCALER_DIV512_gc;				// as fast and as noisy as possible
	
	// temperature sensor
	ADCA.CH0.CTRL = ADC_CH_GAIN_1X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCA.CH0.MUXCTRL = ADC_CH_MUXINT_TEMP_gc;
	ADCA.CH0.INTCTRL = ADC_CH_INTMODE_COMPLETE_gc | ADC_CH_INTLVL_LO_gc;
	ADCA.CH0.CTRL |= ADC_CH_START_bm;
	
	// ADCB
	ADCB.CTRLA = ADC_ENABLE_bm;
	ADCB.CTRLB = ADC_FREERUN_bm | ADC_RESOLUTION_12BIT_gc;
	ADCB.REFCTRL = ADC_REFSEL_INTVCC_gc | ADC_TEMPREF_bm;
	ADCB.EVCTRL = 0;
	ADCB.PRESCALER = ADC_PRESCALER_DIV512_gc;				// as fast and as noisy as possible
	
	// VCC/10
	ADCB.CH0.CTRL = ADC_CH_GAIN_4X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCB.CH0.MUXCTRL = ADC_CH_MUXINT_SCALEDVCC_gc;
	ADCB.CH0.INTCTRL = ADC_CH_INTMODE_COMPLETE_gc | ADC_CH_INTLVL_LO_gc;
	ADCB.CH0.CTRL |= ADC_CH_START_bm;
}

/**************************************************************************************************
** RTC overflow interrupt handler
*/
ISR(RTC_OVF_vect)
{
	static uint8_t byte = 0;
	static uint8_t last = 0;
	static uint8_t bits = 0;
	static bool db = 0;

	ent_bytes++;
	
	db = !db;
	if (db)
		last = FAST_TC.CNTL & 0b1;
	else
	{
		uint8_t fast = FAST_TC.CNTL & 0b1;
		if (fast ^ last)	// von Neumann whitening
		{
			byte <<= 1;
			byte |= fast;
			bits++;
			if (bits > 7)
			{
				DBG_NEW_BIT_OUTPUT;
				entropy[ent_write_head++] = byte;
				if (ent_bytes < 255)
					ent_bytes++;
			}
		}
	}
}

/**************************************************************************************************
** ADCA conversion complete interrupt handler
*/
ISR(ADCA_CH0_vect)
{
	static uint8_t byte = 0;
	static uint8_t last = 0;
	static uint8_t bits = 0;
	static bool db = 0;
	
	db = !db;
	if (db)
		last = ADCA.CH0.RESL & 0b1;
	else
	{
		uint8_t fast = ADCA.CH0.RESL & 0b1;
		if (fast ^ last)	// von Neumann whitening
		{
			byte <<= 1;
			byte |= fast;
			bits++;
			if (bits > 7)
			{
				DBG_NEW_BIT_OUTPUT;
				entropy[ent_write_head++] = byte;
				if (ent_bytes < 255)
					ent_bytes++;
			}
		}
	}
}

/**************************************************************************************************
** ADCB conversion complete interrupt handler
*/
ISR(ADCB_CH0_vect)
{
	static uint8_t byte = 0;
	static uint8_t last = 0;
	static uint8_t bits = 0;
	static bool db = 0;
	
	db = !db;
	if (db)
		last = ADCB.CH0.RESL & 0b1;
	else
	{
		uint8_t fast = ADCB.CH0.RESL & 0b1;
		if (fast ^ last)	// von Neumann whitening
		{
			byte <<= 1;
			byte |= fast;
			bits++;
			if (bits > 7)
			{
				DBG_NEW_BIT_OUTPUT;
				entropy[ent_write_head++] = byte;
				if (ent_bytes < 255)
					ent_bytes++;
			}
		}
	}
}

/**************************************************************************************************
** Full 64 byte buffer with random data
*/
void RND_get_buffer64(uint8_t *buf)
{
	//uint8_t cycles = 4;

	//while (ent_bytes < 80)
	//	sleep_cpu();
	//ent_bytes = 0;
/*
	for (uint8_t i = 0; i < 16; i++)
		AES.KEY = entropy[ent_read_head++];

	while(cycles--)
	{
		//while (!(AES.STATUS & AES_SRIF_bm));
		
		for (uint8_t i = 0; i < 16; i++)
			buf[i] = AES.STATE;
		
		for (uint8_t i = 0; i < 16; i++)
			AES.STATE = entropy[ent_read_head++];
		
		AES.CTRL = AES_START_bm | AES_XOR_bm;
		while (!(AES.STATUS & AES_SRIF_bm));
	}

	buf[1] = ent_bytes;
	buf[0] = 0x55;
*/
	memcpy(buf, (void *)entropy, 64);
}
