#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>

#include "platform.h"

unsigned long platform_seconds()
{
  return (unsigned long)time(0);
}

unsigned short platform_short(unsigned short x)
{
  return ntohs(x);
}

#ifdef DEBUG
int _logging = 1;
#else
int _logging = 0;
#endif

void platform_logging(int enabled)
{
  if(enabled < 0)
  {
    _logging ^= 1;
  }else{
    _logging = enabled;    
  }
  LOG("log output enabled");
}

void *platform_log(const char *file, int line, const char *function, const char * format, ...)
{
  char buffer[256];
  va_list args;
  if(!_logging) return NULL;
  va_start (args, format);
  vsnprintf (buffer, 256, format, args);
  fprintf(stderr,"%s:%d %s() %s\n", file, line, function, buffer);
  fflush(stderr);
  va_end (args);
  return NULL;
}
