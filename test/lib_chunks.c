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

  // try send and receive
  chunks_t c1 = chunks_new(10,2);
  fail_unless(chunks_send(c1, packet));
  fail_unless(chunks_send(c1, packet));
  fail_unless(chunks_waiting(c1) == 230);

  chunks_t c2 = chunks_new(2,1);
  uint8_t *chunk;
  while(chunks_len(c1))
  {
    chunk = chunks_next(c1,&len);
    chunks_chunk(c2,chunk,len);
    chunks_ack(c1,1);
  }
  fail_unless(chunks_waiting(c1) == 0);
  lob_t p1 = chunks_receive(c2);
  fail_unless(p1);
  fail_unless(p1->body_len == 100);
  lob_free(p1);
  lob_t p2 = chunks_receive(c2);
  fail_unless(p2);
  fail_unless(p2->body_len == 100);
  lob_free(p2);

  chunks_free(c1);
  chunks_free(c2);

  return 0;
}

