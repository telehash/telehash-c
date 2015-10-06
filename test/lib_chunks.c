#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  util_chunks_t chunks;

  chunks = util_chunks_new(10);
  fail_unless(chunks);
  fail_unless(util_chunks_len(chunks) == 0);
  fail_unless(!util_chunks_free(chunks));

  chunks = util_chunks_new(10);
  lob_t packet = lob_new();
  lob_body(packet,0,100);
  fail_unless(util_chunks_send(chunks, lob_copy(packet)));
  fail_unless(util_chunks_len(chunks) == 1);
  uint8_t len = 0;

  // check write
  fail_unless(util_chunks_write(chunks));
  LOG("len %d left %d",util_chunks_len(chunks),util_chunks_writing(chunks));
  fail_unless(util_chunks_writing(chunks) == 102);
  fail_unless(util_chunks_written(chunks,1));
  fail_unless(util_chunks_writing(chunks) == 102);
  len = util_chunks_len(chunks);
//  LOG("len %d left %d",util_chunks_len(chunks),util_chunks_writing(chunks));
  fail_unless(len == 9);
  fail_unless(util_chunks_written(chunks,9));
  chunks->blocked = 0; // for testing
//  LOG("len %d left %d",util_chunks_len(chunks),util_chunks_writing(chunks));
  fail_unless(util_chunks_writing(chunks) == 93);

  fail_unless(!util_chunks_free(chunks));

  // try large
  util_chunks_t cbig = util_chunks_new(0);
  lob_t pbig = lob_new();
  lob_body(pbig,0,845);
  fail_unless(util_chunks_send(cbig, lob_copy(pbig)));
  size_t lbig = util_chunks_writing(cbig);
  fail_unless(lbig == 847);
  fail_unless(util_chunks_written(cbig,1));
  cbig->blocked = 0;
  fail_unless(util_chunks_len(cbig) == 255);
  
  // try cloaking
  util_chunks_t cloaked = util_chunks_new(20);
  util_chunks_cloak(cloaked);
  fail_unless(util_chunks_send(cloaked, lob_copy(packet)));
  printf("CLOAK %d\n",util_chunks_writing(cloaked));
  fail_unless(util_chunks_writing(cloaked) == 102);

  // double and unblocked
  util_chunks_t c1 = util_chunks_new(20);
  c1->blocking = 0;
  fail_unless(util_chunks_send(c1, lob_copy(packet)));
  fail_unless(util_chunks_send(c1, lob_copy(packet)));
  fail_unless(util_chunks_writing(c1) == 204);


  // try send and receive by stream
  LOG("chunk stream test");

  util_chunks_t c2 = util_chunks_new(10);
  uint8_t *buf, max;
  for(max=0;max<100;max++)
  {
    if(!(len = util_chunks_len(c1))) break;
    fail_unless((buf = util_chunks_write(c1)));
    util_chunks_read(c2,buf,len);
    fail_unless(util_chunks_written(c1,len));
  }
  LOG("max %d",max);
  fail_unless(max == 2);
  lob_t p1 = util_chunks_receive(c2);
  fail_unless(p1);
  fail_unless(p1->body_len == 100);
  lob_free(p1);
  lob_t p2 = util_chunks_receive(c2);
  fail_unless(p2);
  fail_unless(p2->body_len == 100);
  lob_free(p2);
  util_chunks_free(c1);
  util_chunks_free(c2);

  // blocking behavior
  LOG("chunk blocking test");
  c1 = util_chunks_new(60);
  fail_unless(util_chunks_send(c1, lob_copy(packet)));
  c2 = util_chunks_new(60);
  fail_unless(util_chunks_send(c2, lob_copy(packet)));

  // send first volley
  len = util_chunks_len(c1);
  buf = util_chunks_write(c1);
  fail_unless(util_chunks_read(c2,buf,10));
  fail_unless(util_chunks_read(c2,buf+10,len-10));
  len = util_chunks_len(c2);
  buf = util_chunks_write(c2);
  fail_unless(util_chunks_read(c1,buf,len)); // one chunk back
  fail_unless(util_chunks_receive(c1) == NULL); // no packet yet
  fail_unless(util_chunks_receive(c2) == NULL); // no packet yet
  len = util_chunks_len(c1);
  buf = util_chunks_write(c1);
  fail_unless(util_chunks_read(c2,buf,len)); // rest of packet
  len = util_chunks_len(c2);
  buf = util_chunks_write(c2);
  fail_unless(util_chunks_read(c1,buf,len)); // rest back
  
  p1 = util_chunks_receive(c1);
  fail_unless(p1);
  fail_unless(p1->body_len == 100);
  lob_free(p1);
  p2 = util_chunks_receive(c2);
  fail_unless(p2);
  fail_unless(p2->body_len == 100);
  lob_free(p2);
  util_chunks_free(c1);
  util_chunks_free(c2);


  
  return 0;
}

