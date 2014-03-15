#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>

unsigned long platform_seconds()
{
  return (unsigned long)time(0);
}

unsigned short platform_short(unsigned short x)
{
  return ntohs(x);
}

void platform_debug(char * format, ...)
{
    char buffer[256];
    va_list args;
    va_start (args, format);
    vsprintf (buffer, format, args);
    printf("%s", buffer);
    va_end (args);
}
