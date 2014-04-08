#ifndef platform_h
#define platform_h

// returns a number that increments in seconds for comparison (epoch or just since boot)
unsigned long platform_seconds();

unsigned short platform_short(unsigned short x);

// -1 toggles debug, 0 disable, 1 enable
void platform_debugging(int enabled);
void platform_debug(char * format, ...);

#ifdef DEBUG
#define DEBUG_PRINTF platform_debug
#else
#define DEBUG_PRINTF(...)
#endif

  
#endif