#include "chunks.h"
#include "unit_test.h"
#include "platform.h"

int main(int argc, char **argv)
{
  chunks_t chunks;

  chunks = chunks_new(10);
  fail_unless(chunks);
  fail_unless(chunks_waiting(chunks) == 0);
  fail_unless(!chunks_free(chunks));

  chunks = chunks_new(10);
  lob_t packet = lob_new();
  lob_body(packet,0,100);
  fail_unless(chunks_send(chunks, packet));
  fail_unless(chunks_waiting(chunks) == 115);
  fail_unless(chunks_len(chunks) == 10);
  uint8_t len = 0;
  fail_unless(chunks_out(chunks, &len));
  fail_unless(len == 10);
  fail_unless(chunks_waiting(chunks) == 105);
  
  // check ack
  fail_unless(chunks_next(chunks));
  fail_unless(chunks_waiting(chunks) == 105);
  fail_unless(chunks_next(chunks));
  fail_unless(chunks_waiting(chunks) == 0);
  fail_unless(!chunks_out(chunks, &len));

  // check write
  fail_unless(!chunks_write(chunks));
  fail_unless(chunks_send(chunks, packet));
  fail_unless(chunks_write(chunks));
  fail_unless(chunks_written(chunks,15));
  fail_unless(chunks_waiting(chunks) == 100);

  fail_unless(!chunks_free(chunks));

  // try send and receive by chunk
  chunks_t c1 = chunks_new(20);
  fail_unless(chunks_send(c1, packet));
  fail_unless(chunks_send(c1, packet));
  fail_unless(chunks_waiting(c1) == 218);

  chunks_t c2 = chunks_new(2);
  uint8_t *chunk;
  while(chunks_len(c1))
  {
    fail_unless((chunk = chunks_out(c1,&len)));
    fail_unless(chunks_in(c2,chunk,len));
    fail_unless(chunks_next(c1));
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

  // try send and receive by stream
  LOG("chunk stream test");
  c1 = chunks_new(10);
  fail_unless(chunks_send(c1, packet));
  fail_unless(chunks_send(c1, packet));
  fail_unless(chunks_waiting(c1) == 230);

  c2 = chunks_new(20);
  uint8_t *buf;
  while(chunks_len(c1))
  {
    fail_unless((buf = chunks_write(c1)));
    fail_unless(chunks_read(c2,buf,10));
    fail_unless(chunks_written(c1,10));
  }
  fail_unless(chunks_waiting(c1) > 0);
  p1 = chunks_receive(c2);
  fail_unless(p1);
  fail_unless(p1->body_len == 100);
  lob_free(p1);

  // no p2 yet due to window
  p2 = chunks_receive(c2);
  fail_unless(p2 == NULL);

  // return acks
  fail_unless((buf = chunks_write(c2)));
  fail_unless(chunks_read(c1,buf,chunks_len(c2)));
  fail_unless(chunks_written(c2,chunks_len(c2)));

  // send rest of p2
  fail_unless((buf = chunks_write(c1)));
  fail_unless(chunks_read(c2,buf,chunks_len(c1)));
  fail_unless(chunks_written(c1,chunks_len(c1)));

  p2 = chunks_receive(c2);
  fail_unless(p2);
  fail_unless(p2->body_len == 100);
  lob_free(p2);

  chunks_free(c1);
  chunks_free(c2);

  return 0;
}

