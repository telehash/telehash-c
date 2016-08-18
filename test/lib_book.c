#include "lib.h"
#include "util.h"
#include "e3x.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  chapter_t ch = chapter_create("title",42,42,42);
  fail_unless(ch);
  
  return 0;
}

