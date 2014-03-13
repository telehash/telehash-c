#ifndef platform_h
#define platform_h

#include <stdarg.h>

unsigned short platform_short(unsigned short x);

unsigned long millis(void);
// returns a number that increments in seconds for comparison (epoch or just since boot)
unsigned long platform_seconds();

void DEBUG_(char * format, ...)
{
    char buffer[256];
    va_list args;
    va_start (args, format);
    vsprintf (buffer, format, args);
    printf("%s", buffer);
    va_end (args);
}

#ifdef DEBUG
#define DEBUG_PRINTF(x) DEBUG_ x
 #else
   #define DEBUG_PRINTF(x)
 #endif
#endif
