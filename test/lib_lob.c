#include "lob.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  lob_t packet;
  packet = lob_new();
  fail_unless(packet);

  return 0;
}

