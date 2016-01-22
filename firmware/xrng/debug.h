/*
 * debug.h
 *
 * Created: 21/01/2016 14:38:33
 *  Author: paul.qureshi
 */ 


#ifndef DEBUG_H_
#define DEBUG_H_


#define DEBUG

#ifndef DEBUG
#define DBG_init()
#else
inline static void DBG_init(void)
{
	PORTB.DIRSET = PIN0_bm;
}
#endif


#ifndef DEBUG
#define DBG_NEW_BIT_OUTPUT
#else
#define DBG_NEW_BIT_OUTPUT	PORTB.OUTTGL = PIN0_bm;
#endif



#endif /* DEBUG_H_ */