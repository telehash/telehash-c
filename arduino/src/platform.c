
#include <stdarg.h>
#include "mesh.h"

unsigned long platform_seconds()
{
  return (unsigned long)millis()/1000;
}

unsigned short platform_short(unsigned short x)
{
   return ( ((x)<<8) | (((x)>>8)&0xFF) );
}

void platform_random_init(void)
{
  srandom(analogRead(0));
}

long platform_random(void)
{
  // TODO use hardware random
  return random();
}

int _debugging = 0;
void platform_logging(int enabled)
{
  if(enabled < 0)
  {
    _debugging ^= 1;
  }else{
    _debugging = enabled;    
  }
  LOG("debug output enabled");
}

void *platform_log(const char *file, int line, const char *function, const char * format, ...)
{
    char buffer[256];
    va_list args;
    if(!_debugging) return NULL;
    va_start (args, format);
    vsnprintf (buffer, 256, format, args);
    println(buffer);
    va_end (args);
    return NULL;
}
