unsigned long platform_seconds()
{
  return (unsigned long)millis()/1000;
}

unsigned short platform_short(unsigned short x)
{
   return ( ((x)<<8) | (((x)>>8)&0xFF) );
}