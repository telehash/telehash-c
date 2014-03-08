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