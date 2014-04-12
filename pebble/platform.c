#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "platform.h"

unsigned long platform_seconds()
{
  return (unsigned long)time(0);
}

unsigned short platform_short(unsigned short x)
{
  return ( ((x)<<8) | (((x)>>8)&0xFF) );
}

int _debugging = 0;
void platform_debugging(int enabled)
{
  if(enabled < 0)
  {
    _debugging ^= 1;
  }else{
    _debugging = enabled;    
  }
  DEBUG_PRINTF("debug output enabled");
}

void platform_debug(char * format, ...)
{
    char buffer[256];
    va_list args;
    if(!_debugging) return;
    va_start (args, format);
    vsnprintf (buffer, 256, format, args);
//    printf("%s\n", buffer);
    va_end (args);
}

void *_sbrk(int incr)
{
  return NULL;
}