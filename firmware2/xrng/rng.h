/*
 * rng.h
 *
 */


#ifndef RNG_H_
#define RNG_H_


#define FAST_TC		TCC1


extern void RNG_init(void);
extern void RNG_get_buffer(uint8_t *buf, uint16_t buffer_size);


#endif /* RNG_H_ */