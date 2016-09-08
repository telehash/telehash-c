#include "telehash.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(!e3x_init(NULL)); // random seed
  
  gossip_t g = gossip_new();
  fail_unless(g);
  fail_unless(g->x == 1);

  return 0;
}

