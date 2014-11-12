#include "chunks.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  chunks_t chunks;

  chunks = chunks_new(1,1);
  fail_unless(chunks);
  fail_unless(!chunks_free(chunks));

  return 0;
}

