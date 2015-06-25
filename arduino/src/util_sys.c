
#include <stdarg.h>
#include "telehash.h"

unsigned long util_sys_seconds()
{
  return (unsigned long)millis()/1000;
}

unsigned short util_sys_short(unsigned short x)
{
   return ( ((x)<<8) | (((x)>>8)&0xFF) );
}

void util_sys_random_init(void)
{
  srandom(analogRead(0));
}

long util_sys_random(void)
{
  // TODO use hardware random
  return PHY_RandomReq();
//  return random();
}

int _debugging = 0;
void util_sys_logging(int enabled)
{
  if(enabled < 0)
  {
    _debugging ^= 1;
  }else{
    _debugging = enabled;    
  }
  LOG("debug output enabled");
}

void *util_sys_log(const char *file, int line, const char *function, const char * format, ...)
{
    char buffer[256];
    va_list args;
    if(!_debugging) return NULL;
    va_start (args, format);
    vsnprintf (buffer, 256, format, args);
    printf("%s:%d %s() %s\n", file, line, function, buffer);
    va_end (args);
    return NULL;
}
