#include "chunks.h"
#include "unit_test.h"
#include "platform.h"

int main(int argc, char **argv)
{
  chunks_t chunks;

  chunks = chunks_new(10);
  fail_unless(chunks);
  fail_unless(chunks_len(chunks) == 0);
  fail_unless(!chunks_free(chunks));

  chunks = chunks_new(10);
  lob_t packet = lob_new();
  lob_body(packet,0,100);
  fail_unless(chunks_send(chunks, packet));
  fail_unless(chunks_len(chunks) == 115);
  uint8_t len = 0;
  fail_unless(chunks_out(chunks, &len));
  fail_unless(len == 10);
  fail_unless(chunks_len(chunks) == 105);
  fail_unless(chunks_out(chunks, &len));
  fail_unless(len == 10);
  fail_unless(chunks_len(chunks) == 95);
  while(chunks_len(chunks)) fail_unless(chunks_out(chunks, &len));

  // check write
  fail_unless(!chunks_write(chunks));
  fail_unless(chunks_send(chunks, packet));
  fail_unless(chunks_write(chunks));
  fail_unless(chunks_len(chunks) == 115);
  fail_unless(chunks_written(chunks,15));
  fail_unless(chunks_len(chunks) == 100);

  fail_unless(!chunks_free(chunks));

  // try cloaking
  chunks_t cloaked = chunks_new(20);
  chunks_cloak(cloaked);
  fail_unless(chunks_send(cloaked, packet));
  printf("CLOAK %d\n",chunks_len(cloaked));
  fail_unless(chunks_len(cloaked) == 117);

  // try send and receive by chunk
  chunks_t c1 = chunks_new(20);
  fail_unless(chunks_send(c1, packet));
  fail_unless(chunks_send(c1, packet));
  fail_unless(chunks_len(c1) == 218);

  chunks_t c2 = chunks_new(2);
  uint8_t *chunk;
  while(chunks_len(c1))
  {
    fail_unless((chunk = chunks_out(c1,&len)));
    fail_unless(chunks_in(c2,chunk,len));
  }
  fail_unless(chunks_len(c1) == 0);
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
  fail_unless(chunks_len(c1) == 230);

  c2 = chunks_new(20);
  uint8_t *buf;
  while(chunks_len(c1))
  {
    fail_unless((buf = chunks_write(c1)));
    chunks_read(c2,buf,10);
    fail_unless(chunks_written(c1,10));
  }
  p1 = chunks_receive(c2);
  fail_unless(p1);
  fail_unless(p1->body_len == 100);
  lob_free(p1);
  p2 = chunks_receive(c2);
  fail_unless(p2);
  fail_unless(p2->body_len == 100);
  lob_free(p2);
  chunks_free(c1);
  chunks_free(c2);

  // blocking behavior
  LOG("chunk blocking test");
  c1 = chunks_new(60);
  fail_unless(chunks_send(c1, packet));
  c2 = chunks_new(60);
  fail_unless(chunks_send(c2, packet));

  // send first volley
  buf = chunks_out(c1, &len);
  fail_unless(chunks_read(c2,buf,10) == NULL); // incomplete
  fail_unless(chunks_read(c2,buf+10,len-10)); // one chunk
  buf = chunks_out(c2, &len);
  fail_unless(chunks_read(c1,buf,len)); // one chunk back
  fail_unless(chunks_receive(c1) == NULL); // no packet yet
  fail_unless(chunks_receive(c2) == NULL); // no packet yet
  buf = chunks_out(c1, &len);
  fail_unless(chunks_read(c2,buf,len)); // rest of packet
  buf = chunks_out(c2, &len);
  fail_unless(chunks_read(c1,buf,len)); // rest back
  
  p1 = chunks_receive(c1);
  fail_unless(p1);
  fail_unless(p1->body_len == 100);
  lob_free(p1);
  p2 = chunks_receive(c2);
  fail_unless(p2);
  fail_unless(p2->body_len == 100);
  lob_free(p2);
  chunks_free(c1);
  chunks_free(c2);

  return 0;
}

