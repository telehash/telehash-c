#ifndef platform_h
#define platform_h

unsigned short platform_short(unsigned short x);

unsigned long millis(void);
// returns a number that increments in seconds for comparison (epoch or just since boot)
unsigned long platform_seconds();

#endif
