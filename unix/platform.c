#include <time.h>

unsigned long platform_seconds()
{
  return (unsigned long)time(0);
}
