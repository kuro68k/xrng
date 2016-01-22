/*
 * rng.h
 *
 * Created: 12/01/2016 16:27:41
 *  Author: paul.qureshi
 */ 


#ifndef RNG_H_
#define RNG_H_


#define FAST_TC		TCC1


extern void RNG_init(void);
extern void RND_get_buffer64(uint8_t *buf);


#endif /* RNG_H_ */