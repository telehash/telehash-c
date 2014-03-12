#include <stdio.h>
#include <stdlib.h>
#include <time.h>

unsigned short platform_short(unsigned short x)
{
   return ( ((x)<<8) | (((x)>>8)&0xFF) );
}

unsigned long millis(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts );
    return ( ts.tv_sec * 1000 + ts.tv_nsec / 1000000L );
}

unsigned long platform_seconds()
{
  return (unsigned long)millis()/1000;
}
