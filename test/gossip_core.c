#include "telehash.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(!e3x_init(NULL)); // random seed
  
  gossip_t g = gossip_new();
  fail_unless(g);
  g->type = 1;
  g->head = 2;
  g->body = 42;
  char hex[16];
  util_hex(g->bin,5,hex);
  printf("hex is %s\n",hex);
  fail_unless(strcmp(hex,"110000002a") == 0);
  
  fail_unless(!gossip_free(g));

  return 0;
}

