#include "xht.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  xht_t h;
  fail_unless((h = xht_new(11)));
  fail_unless(xht_get(h,"key") == NULL);
  xht_set(h,"key","value");
  fail_unless(xht_get(h,"key"));
  xht_set(h,"key","value2");
  fail_unless(strcmp(xht_get(h,"key"),"value2") == 0);
  xht_set(h,"key2","value2");
  
  char *key = NULL;
  int i=0;
  while((key = xht_iter(h,key))) i++;
  fail_unless(i == 2);

  return 0;
}

