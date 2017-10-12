/*
 * rng.c
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <string.h>
#include <stdlib.h>
#include "global.h"
//#include "debug.h"
//#include "hw_misc.h"
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
	//for (uint8_t i = 0; i < 16; i++)
	//	AES.KEY = i;

	// fast/slow out of sync counters
	CLK.RTCCTRL = CLK_RTCSRC_ULP_gc | CLK_RTCEN_bm;		// slow, jittery 1024Hz
	//CLK.RTCCTRL = (0b100 << CLK_RTCSRC_gp) | CLK_RTCEN_bm;	// slow, jittery 32768Hz
	RTC.CTRL = 0;
	while (RTC.STATUS & RTC_SYNCBUSY_bm);
	RTC.PER = 1;
	RTC.CNT = 0;
	//RTC.INTCTRL = RTC_OVFINTLVL_LO_gc;
	RTC.CTRL = RTC_PRESCALER_DIV1_gc;

	FAST_TC.CTRLA = 0;
	FAST_TC.CTRLB = 0;
	FAST_TC.CTRLC = 0;
	FAST_TC.CTRLD = 0;
	FAST_TC.CTRLE = 0;
	FAST_TC.PER = 0xFFFF;
	FAST_TC.CTRLA = TC_CLKSEL_DIV1_gc;					// fast, from CPU clock

	// DAC
	DACB.CTRLA = DAC_IDOEN_bm | DAC_CH1EN_bm | DAC_CH0EN_bm | DAC_ENABLE_bm;
	DACB.CTRLB = DAC_CHSEL_DUAL_gc;
	DACB.CTRLC = DAC_REFSEL_INT1V_gc;

	// ADCA
	ADCA.CTRLA = ADC_ENABLE_bm;
	ADCA.CTRLB = ADC_FREERUN_bm | ADC_RESOLUTION_12BIT_gc;
	ADCA.REFCTRL = ADC_REFSEL_INTVCC_gc | ADC_TEMPREF_bm;
	ADCA.EVCTRL = ADC_SWEEP_0123_gc;
	ADCA.PRESCALER = ADC_PRESCALER_DIV16_gc;			// as fast and as noisy as possible

	// temperature sensor
	ADCA.CH0.CTRL = ADC_CH_GAIN_1X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCA.CH0.MUXCTRL = ADC_CH_MUXINT_TEMP_gc;
	//ADCA.CH0.INTCTRL = ADC_CH_INTMODE_COMPLETE_gc | ADC_CH_INTLVL_LO_gc;
	//ADCA.CH0.CTRL |= ADC_CH_START_bm;
	ADCA.CH1.CTRL = ADC_CH_GAIN_1X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCA.CH1.MUXCTRL = ADC_CH_MUXINT_TEMP_gc;
	ADCA.CH2.CTRL = ADC_CH_GAIN_1X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCA.CH2.MUXCTRL = ADC_CH_MUXINT_SCALEDVCC_gc;
	ADCA.CH3.CTRL = ADC_CH_GAIN_1X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCA.CH3.MUXCTRL = ADC_CH_MUXINT_DAC_gc;

	// ADCB
	ADCB.CTRLA = ADC_ENABLE_bm;
	ADCB.CTRLB = ADC_FREERUN_bm | ADC_RESOLUTION_12BIT_gc;
	ADCB.REFCTRL = ADC_REFSEL_INTVCC_gc | ADC_TEMPREF_bm;
	ADCB.EVCTRL = ADC_SWEEP_0123_gc;
	ADCB.PRESCALER = ADC_PRESCALER_DIV16_gc;			// as fast and as noisy as possible

	// VCC/10
	ADCB.CH0.CTRL = ADC_CH_GAIN_4X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCB.CH0.MUXCTRL = ADC_CH_MUXINT_SCALEDVCC_gc;
	//ADCB.CH0.INTCTRL = ADC_CH_INTMODE_COMPLETE_gc | ADC_CH_INTLVL_LO_gc;
	//ADCB.CH0.CTRL |= ADC_CH_START_bm;
	ADCB.CH1.CTRL = ADC_CH_GAIN_4X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCB.CH1.MUXCTRL = ADC_CH_MUXINT_SCALEDVCC_gc;
	ADCB.CH2.CTRL = ADC_CH_GAIN_4X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCB.CH2.MUXCTRL = ADC_CH_MUXINT_DAC_gc;
	ADCB.CH3.CTRL = ADC_CH_GAIN_4X_gc | ADC_CH_INPUTMODE_INTERNAL_gc;
	ADCB.CH3.MUXCTRL = ADC_CH_MUXINT_TEMP_gc;

	// CRC peripheral
	CRC.CTRL = CRC_CRC32_bm | CRC_SOURCE_IO_gc;
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
//				DBG_NEW_BIT_OUTPUT;
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
//				DBG_NEW_BIT_OUTPUT;
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
//				DBG_NEW_BIT_OUTPUT;
				entropy[ent_write_head++] = byte;
				if (ent_bytes < 255)
					ent_bytes++;
			}
		}
	}
}

/**************************************************************************************************
** Full buffer with random data
*/
void RNG_get_buffer(uint8_t *buf, uint16_t buffer_size)
{
	//uint16_t		acc = 0;

	uint16_t byte_count = 0;
	//uint8_t white_bits = 0;
	uint8_t white_byte = 0;
	uint8_t	new_bits = 0;
	uint8_t	nu = 0;

	WDR();

	// AES - consistent bias towards zero
	while (buffer_size)
	{
		for(uint8_t i = 0; i < 16; i++)
		{
			while (!(ADCA.INTFLAGS & ADC_CH0IF_bm));
			ADCA.INTFLAGS = ADC_CH0IF_bm;	// clear bit
			AES.STATE = (ADCA.CH0RESL & 0x0F) | (ADCA.CH1RESL >> 4);
			AES.KEY = (ADCB.CH2RESL & 0x0F) | (ADCA.CH3RESL >> 4);
		}
		AES.CTRL |= AES_START_bm;
		while(!(AES.STATUS & AES_SRIF_bm));
		for(uint8_t i = 0; i < 16; i++)
		{
			CRC.DATAIN = AES.STATE;	// CRC32 whitening
			*buf++ = CRC.CHECKSUM0;
		}
		buffer_size -= 16;
	}
	return;

	// multi channel
	for(;;)
	{
		while (!(ADCA.INTFLAGS & ADC_CH0IF_bm));
		ADCA.INTFLAGS = ADC_CH0IF_bm;	// clear bit

		uint8_t b = ADCA.CH0RESL & 1;
		b <<= 1;
		b |= ADCA.CH1RESL & 1;
		b <<= 1;
		b |= ADCA.CH2RESL & 1;
		b <<= 1;
		b |= ADCA.CH3RESL & 1;

		b <<= 1;
		b |= ADCB.CH0RESL & 1;
		b <<= 1;
		b |= ADCB.CH1RESL & 1;
		b <<= 1;
		b |= ADCB.CH2RESL & 1;
		b <<= 1;
		b |= ADCB.CH3RESL & 1;
		CRC.DATAIN = b;

		DACB.CH0DATA = CRC.CHECKSUM3 | (CRC.CHECKSUM2 << 8);
		DACB.CH1DATA = CRC.CHECKSUM1 | (CRC.CHECKSUM0 << 8);

/*
		while (!(ADCA.INTFLAGS & ADC_CH0IF_bm));
		ADCA.INTFLAGS = ADC_CH0IF_bm;	// clear bit
		b <<= 1;
		b |= ADCA.CH0RESL & 1;
		b <<= 1;
		b |= ADCA.CH1RESL & 1;
		b <<= 1;
		b |= ADCA.CH2RESL & 1;
		b <<= 1;
		b |= ADCA.CH3RESL & 1;
		b <<= 1;
		b |= ADCB.CH0RESL & 1;
		b <<= 1;
		b |= ADCB.CH1RESL & 1;
		b <<= 1;
		b |= ADCB.CH2RESL & 1;
		b <<= 1;
		b |= ADCB.CH3RESL & 1;
*/
		CRC.DATAIN = b;	// CRC32 whitening
		*buf++ = CRC.CHECKSUM0;
		byte_count++;
		if (byte_count > buffer_size)
			return;
	}



	for(;;)
	{
		// ADC A
		if (ADCA.INTFLAGS & ADC_CH0IF_bm)
		{
			ADCA.INTFLAGS = ADC_CH0IF_bm;	// clear bit
			CRC.DATAIN = ADCA.CH0.RESL;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		if (ADCA.INTFLAGS & ADC_CH1IF_bm)
		{
			ADCA.INTFLAGS = ADC_CH1IF_bm;	// clear bit
			CRC.DATAIN = ADCA.CH1.RESL;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		if (ADCA.INTFLAGS & ADC_CH2IF_bm)
		{
			ADCA.INTFLAGS = ADC_CH2IF_bm;	// clear bit
			CRC.DATAIN = ADCA.CH2.RESL;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		if (ADCA.INTFLAGS & ADC_CH3IF_bm)
		{
			ADCA.INTFLAGS = ADC_CH3IF_bm;	// clear bit
			CRC.DATAIN = ADCA.CH3.RESL;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		// ADC B
		if (ADCB.INTFLAGS & ADC_CH0IF_bm)
		{
			ADCB.INTFLAGS = ADC_CH0IF_bm;	// clear bit
			CRC.DATAIN = ADCB.CH0.RESL;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		if (ADCB.INTFLAGS & ADC_CH1IF_bm)
		{
			ADCB.INTFLAGS = ADC_CH1IF_bm;	// clear bit
			CRC.DATAIN = ADCB.CH1.RESL;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		if (ADCB.INTFLAGS & ADC_CH2IF_bm)
		{
			ADCB.INTFLAGS = ADC_CH2IF_bm;	// clear bit
			CRC.DATAIN = ADCB.CH2.RESL;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		if (ADCB.INTFLAGS & ADC_CH3IF_bm)
		{
			ADCB.INTFLAGS = ADC_CH3IF_bm;	// clear bit
			CRC.DATAIN = ADCB.CH3.RESL;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}
	}

	for(;;)
	{
		// ADCA
		if (ADCA.INTFLAGS & ADC_CH0IF_bm)
		{
			nu <<= 1;
			nu |= ADCA.CH0.RESL & 1;
			ADCA.INTFLAGS = ADC_CH0IF_bm;	// clear bit
			new_bits++;
		}

		if (ADCA.INTFLAGS & ADC_CH1IF_bm)
		{
			nu <<= 1;
			nu |= ADCA.CH1.RESL & 1;
			ADCA.INTFLAGS = ADC_CH1IF_bm;	// clear bit
			new_bits++;
		}

		if (new_bits >= 8)
		{
			new_bits = 0;
			CRC.DATAIN = white_byte;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		if (ADCA.INTFLAGS & ADC_CH2IF_bm)
		{
			nu <<= 1;
			nu |= ADCA.CH2.RESL & 1;
			ADCA.INTFLAGS = ADC_CH2IF_bm;	// clear bit
			new_bits++;
		}

		if (ADCA.INTFLAGS & ADC_CH3IF_bm)
		{
			nu <<= 1;
			nu |= ADCA.CH3.RESL & 1;
			ADCA.INTFLAGS = ADC_CH3IF_bm;	// clear bit
			new_bits++;
		}

		if (new_bits >= 8)
		{
			new_bits = 0;
			CRC.DATAIN = white_byte;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		// ADCB
		if (ADCB.INTFLAGS & ADC_CH0IF_bm)
		{
			nu <<= 1;
			nu |= ADCB.CH0.RESL & 1;
			ADCB.INTFLAGS = ADC_CH0IF_bm;	// clear bit
			new_bits++;
		}

		if (ADCB.INTFLAGS & ADC_CH1IF_bm)
		{
			nu <<= 1;
			nu |= ADCB.CH1.RESL & 1;
			ADCB.INTFLAGS = ADC_CH1IF_bm;	// clear bit
			new_bits++;
		}

		if (new_bits >= 8)
		{
			new_bits = 0;
			CRC.DATAIN = white_byte;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

		if (ADCB.INTFLAGS & ADC_CH2IF_bm)
		{
			nu <<= 1;
			nu |= ADCB.CH2.RESL & 1;
			ADCB.INTFLAGS = ADC_CH2IF_bm;	// clear bit
			new_bits++;
		}

		if (ADCB.INTFLAGS & ADC_CH3IF_bm)
		{
			nu <<= 1;
			nu |= ADCB.CH3.RESL & 1;
			ADCB.INTFLAGS = ADC_CH3IF_bm;	// clear bit
			new_bits++;
		}

		if (new_bits >= 8)
		{
			new_bits = 0;
			CRC.DATAIN = white_byte;	// CRC32 whitening
			*buf = CRC.CHECKSUM0;
			byte_count++;
			if (byte_count > buffer_size)
				return;
		}

/*
		while (new_bits > 0)
		{
			new_bits--;

			white_byte <<= 1;
			white_byte |= nu & 1;
			nu >>= 1;
			white_bits++;

			if (white_bits >= 8)
			{
				white_bits = 0;

				//srand(white_byte);
				// *buf = rand() & 0xFF;

				CRC.DATAIN = white_byte;	// CRC32 whitening
				*buf = CRC.CHECKSUM0;

				// *buf = white_byte;

				acc += *buf++;
				byte_count++;
				if (byte_count > buffer_size)
					return;
			}
		}
*/
/*
		// von neumann whitening
		//if (new_bits > 2)
		while (new_bits > 2)
		{
			new_bits -= 2;

			uint8_t b = nu & 0b11;
			nu >>= 2;
			if ((b != 0b00) & (b != 0b11))
			{
				white_byte <<= 1;
				white_byte |= b & 1;
				white_bits++;
				if (white_bits > 8)
				{
					CRC.DATAIN = white_byte;	// CRC32 whitening
					*buf++ = CRC.CHECKSUM0;
					// *buf++ = white_byte;
					byte_count++;
					if (byte_count > 63)
						return;
					white_bits = 0;
				}
			}
		}
*/
	}
}
