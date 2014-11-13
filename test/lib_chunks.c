#include "chunks.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  chunks_t chunks;

  chunks = chunks_new(10,1);
  fail_unless(chunks);
  fail_unless(chunks_waiting(chunks) == 0);
  fail_unless(!chunks_free(chunks));

  chunks = chunks_new(10,1);
  lob_t packet = lob_new();
  lob_body(packet,0,100);
  fail_unless(chunks_send(chunks, packet));
  fail_unless(chunks_waiting(chunks) == 115);
  fail_unless(chunks_len(chunks) == 10);
  uint8_t len = 0;
  fail_unless(chunks_next(chunks, &len));
  fail_unless(len == 10);
  fail_unless(chunks_waiting(chunks) == 105);
  
  // check ack
  fail_unless(chunks_ack(chunks, 1));
  fail_unless(chunks_waiting(chunks) == 105);
  fail_unless(chunks_ack(chunks, 20));
  fail_unless(chunks_waiting(chunks) == 0);
  fail_unless(!chunks_next(chunks, &len));

  // check write
  fail_unless(!chunks_write(chunks));
  fail_unless(chunks_send(chunks, packet));
  fail_unless(chunks_write(chunks));
  fail_unless(chunks_written(chunks,15));
  fail_unless(chunks_waiting(chunks) == 100);

  fail_unless(!chunks_free(chunks));

  return 0;
}

