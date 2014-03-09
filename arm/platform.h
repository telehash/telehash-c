#ifndef platform_h
#define platform_h

unsigned short platform_short(unsigned short x);

unsigned long millis(void);
// returns a number that increments in seconds for comparison (epoch or just since boot)
unsigned long platform_seconds();

#define CS_1a 1 //what cs to use. in here so it gets propogated to all files. really 
		// should be in the crypt_xx.h/c file but will do here for the time
		// being 

#endif
