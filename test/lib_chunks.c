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
  fail_unless(util_chunks_out(chunks, &len));
//  LOG("len %d",len);
  fail_unless(len == 9);
  fail_unless(util_chunks_len(chunks) == 1);
  util_chunks_next(chunks);
  fail_unless(util_chunks_out(chunks, &len));
  fail_unless(len == 9);
  LOG("uwrite %d",util_chunks_writing(chunks));
  fail_unless(util_chunks_writing(chunks) == 102);
  int max;
  for(max=0;max<10;max++)
  {
    if(!util_chunks_len(chunks)) break;
    fail_unless(util_chunks_next(chunks) && util_chunks_out(chunks, &len));
  }
  LOG("max %d",max);
  fail_unless(max == 3);

  // check write
  fail_unless(!util_chunks_write(chunks));
  fail_unless(util_chunks_send(chunks, lob_copy(packet)));
  fail_unless(util_chunks_write(chunks));
  fail_unless(util_chunks_writing(chunks) == 115);
  fail_unless(util_chunks_written(chunks,15));
  fail_unless(util_chunks_writing(chunks) == 100);

  fail_unless(!util_chunks_free(chunks));

  // try large
  util_chunks_t cbig = util_chunks_new(0);
  lob_t pbig = lob_new();
  lob_body(pbig,0,845);
  fail_unless(util_chunks_send(cbig, lob_copy(pbig)));
  size_t lbig = util_chunks_writing(cbig);
  fail_unless(lbig == 852);
  fail_unless(util_chunks_written(cbig,lbig));
  
  // try cloaking
  util_chunks_t cloaked = util_chunks_new(20);
  util_chunks_cloak(cloaked);
  fail_unless(util_chunks_send(cloaked, lob_copy(packet)));
  printf("CLOAK %d\n",util_chunks_len(cloaked));
  fail_unless(util_chunks_writing(cloaked) == 117);

  // try send and receive by chunk
  util_chunks_t c1 = util_chunks_new(20);
  fail_unless(util_chunks_send(c1, lob_copy(packet)));
  fail_unless(util_chunks_send(c1, lob_copy(packet)));
  fail_unless(util_chunks_writing(c1) == 218);

  util_chunks_t c2 = util_chunks_new(2);
  uint8_t *chunk;
  for(max=0;max<10;max++)
  {
    if(!util_chunks_len(c1)) break;
    fail_unless((chunk = util_chunks_out(c1,&len)));
    fail_unless(util_chunks_in(c2,chunk,len));
    util_chunks_next(c1);
  }
  LOG("max %d",max);
  fail_unless(max == 3);
  fail_unless(util_chunks_len(c1) == 0);
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

  // try send and receive by stream
  LOG("chunk stream test");
  c1 = util_chunks_new(10);
  fail_unless(util_chunks_send(c1, lob_copy(packet)));
  fail_unless(util_chunks_send(c1, lob_copy(packet)));
  fail_unless(util_chunks_writing(c1) == 230);

  c2 = util_chunks_new(20);
  uint8_t *buf;
  for(max=0;max<10;max++)
  {
    if(!util_chunks_len(c1)) break;
    fail_unless((buf = util_chunks_write(c1)));
    util_chunks_read(c2,buf,10);
    fail_unless(util_chunks_written(c1,10));
  }
  LOG("max %d",max);
  fail_unless(max == 3);
  p1 = util_chunks_receive(c2);
  fail_unless(p1);
  fail_unless(p1->body_len == 100);
  lob_free(p1);
  p2 = util_chunks_receive(c2);
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
  buf = util_chunks_out(c1, &len);
  fail_unless(util_chunks_read(c2,buf,10));
  fail_unless(util_chunks_ack(c2) == NULL); // incomplete
  fail_unless(util_chunks_read(c2,buf+10,len-10));
  fail_unless(util_chunks_ack(c2)); // one chunk
  buf = util_chunks_out(c2, &len);
  fail_unless(util_chunks_read(c1,buf,len)); // one chunk back
  fail_unless(util_chunks_ack(c1));
  fail_unless(util_chunks_receive(c1) == NULL); // no packet yet
  fail_unless(util_chunks_receive(c2) == NULL); // no packet yet
  buf = util_chunks_out(c1, &len);
  fail_unless(util_chunks_read(c2,buf,len)); // rest of packet
  fail_unless(util_chunks_ack(c2));
  buf = util_chunks_out(c2, &len);
  fail_unless(util_chunks_read(c1,buf,len)); // rest back
  fail_unless(util_chunks_ack(c1));
  
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

  // reproduce a gc bug
  util_chunks_t c3 = util_chunks_new(32);
  lob_t blob = lob_new();
  lob_body(blob,NULL,50);
  int i;
  for(i=0;i<10;i++)
  {
    fail_unless(util_chunks_send(c3,lob_copy(blob)));
    len = 1;
    fail_unless(util_chunks_out(c3,&len));
    fail_unless(len == 32);
    fail_unless(util_chunks_out(c3,&len) == NULL); // blocked
    fail_unless(len == 0);
    fail_unless(util_chunks_next(c3)); // unblock
    fail_unless(util_chunks_out(c3,&len));
    fail_unless(len == ((52-32)+3));
    fail_unless(util_chunks_next(c3)); // unblock
    fail_unless(util_chunks_out(c3,&len) == NULL);
    fail_unless(len == 0);
  }

  fail_unless(util_chunks_send(c3,lob_copy(blob)));
  len = 1;
  fail_unless(util_chunks_out(c3,&len));
  fail_unless(len == 32);
  fail_unless(util_chunks_out(c3,&len) == NULL); // blocked
  fail_unless(len == 0);
  fail_unless(util_chunks_next(c3)); // unblock
  fail_unless(util_chunks_out(c3,&len));
  fail_unless(len == ((52-32)+3));
  fail_unless(util_chunks_send(c3,lob_copy(blob)));
  fail_unless(util_chunks_next(c3)); // unblock
  fail_unless(util_chunks_out(c3,&len));
  fail_unless(len == 32);
  fail_unless(util_chunks_out(c3,&len) == NULL);
  fail_unless(len == 0);
  
  return 0;
}

