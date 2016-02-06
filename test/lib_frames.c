#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  util_frames_t frames;

  frames = util_frames_new(10);
  fail_unless(frames);
  fail_unless(util_frames_len(frames) == 0);
  fail_unless(!util_frames_free(frames));

  frames = util_frames_new(10);
  lob_t packet = lob_new();
  lob_body(packet,0,100);
  fail_unless(util_frames_send(frames, lob_copy(packet)));
  fail_unless(util_frames_len(frames) == 1);
  uint8_t len = 0;

  // check write
  fail_unless(util_frames_write(frames));
  LOG("wait %d write %d",frames->waitat,frames->writeat);
  fail_unless(frames->waitat == 0);
  fail_unless(frames->writeat == 0);
  fail_unless(util_frames_written(frames,1));
  LOG("wait %d write %d",frames->waitat,frames->writeat);
  fail_unless(frames->waitat == 1);
  fail_unless(frames->writeat == 0);
  len = util_frames_len(frames);
  fail_unless(len == 9);
  fail_unless(util_frames_written(frames,4));
  fail_unless(frames->waitat == 5);
  fail_unless(frames->writeat == 0);
  fail_unless(util_frames_len(frames) == 5);
  fail_unless(util_frames_written(frames,5));
  fail_unless(frames->waitat == 0);
  fail_unless(frames->writeat == 9);

  frames->blocked = 0; // for testing

  fail_unless(!util_frames_free(frames));

  // try large
  util_frames_t cbig = util_frames_new(0);
  lob_t pbig = lob_new();
  lob_body(pbig,0,845);
  fail_unless(util_frames_send(cbig, lob_copy(pbig)));
  fail_unless(util_frames_written(cbig,1));
  cbig->blocked = 0;
  fail_unless(util_frames_len(cbig) == 255);
  
  // double and unblocked
  util_frames_t c1 = util_frames_new(20);
  c1->blocking = 0;
  fail_unless(util_frames_send(c1, lob_copy(packet)));
  fail_unless(util_frames_send(c1, lob_copy(packet)));


  // try send and receive by stream
  LOG("frame stream test");

  util_frames_t c2 = util_frames_new(20);
  uint8_t *buf, max;
  for(max=0;max<100;max++)
  {
    if(!(len = util_frames_len(c1))) break;
    LOG("len %d",len);
    fail_unless((buf = util_frames_write(c1)));
    util_frames_read(c2,buf,len);
    fail_unless(util_frames_written(c1,len));
  }
  LOG("max %d",max);
  fail_unless(max == 26);
  lob_t p1 = util_frames_receive(c2);
  fail_unless(p1);
  LOG("len %d",p1->body_len);
  fail_unless(p1->body_len == 100);
  lob_free(p1);
  lob_t p2 = util_frames_receive(c2);
  fail_unless(p2);
  fail_unless(p2->body_len == 100);
  lob_free(p2);
  util_frames_free(c1);
  util_frames_free(c2);

  // blocking behavior
  LOG("frame blocking test");
  c1 = util_frames_new(60);
  fail_unless(util_frames_send(c1, lob_copy(packet)));
  c2 = util_frames_new(60);
  fail_unless(util_frames_send(c2, lob_copy(packet)));

  // send first volley
  len = util_frames_len(c1);
  buf = util_frames_write(c1);
  fail_unless(util_frames_read(c2,buf,len));
  fail_unless(util_frames_written(c1,len));
  len = util_frames_len(c2);
  buf = util_frames_write(c2);
  fail_unless(util_frames_read(c1,buf,len)); // one frame back
  fail_unless(util_frames_written(c2,len));
  fail_unless(util_frames_receive(c1) == NULL); // no packet yet
  fail_unless(util_frames_receive(c2) == NULL); // no packet yet
  
  // rest of packet
  size_t len1, len2;
  for(max=0;max<20;max++)
  {
    len1 = util_frames_len(c1);
    LOG("len %d blk %d ack %d",len1,c1->blocked,c1->ack);
    buf = util_frames_write(c1);
    fail_unless(util_frames_read(c2,buf,len1));
    fail_unless(util_frames_written(c1,len1));
    len2 = util_frames_len(c2);
    LOG("len %d blk %d ack %d",len2,c2->blocked,c2->ack);
    buf = util_frames_write(c2);
    fail_unless(util_frames_read(c1,buf,len2));
    fail_unless(util_frames_written(c2,len2));
    if(!len1 && !len2) break;
  }
  LOG("max of %d",max);
  fail_unless(max == 4);
  
  p1 = util_frames_receive(c1);
  fail_unless(p1);
  fail_unless(p1->body_len == 100);
  lob_free(p1);
  p2 = util_frames_receive(c2);
  fail_unless(p2);
  fail_unless(p2->body_len == 100);
  lob_free(p2);
  util_frames_free(c1);
  util_frames_free(c2);

  LOG("testing frame-based frameing");
  util_frames_t f1 = util_frames_new(10);
  fail_unless(f1);
  fail_unless(util_frames_send(f1, lob_copy(packet)));
  fail_unless(util_frames_size(f1) == 9);
  fail_unless(util_frames_frame(f1));
  fail_unless(util_frames_peek(f1));
  while(util_frames_size(f1) == 9) fail_unless(util_frames_next(f1));
  LOG("tail is size %d",util_frames_size(f1));
  fail_unless(util_frames_size(f1) == 3);
  fail_unless(util_frames_peek(f1) == 0);
  fail_unless(util_frames_size(f1) == 3);
  fail_unless(util_frames_next(f1));
  fail_unless(util_frames_size(f1) == 0);
  fail_unless(util_frames_peek(f1) == -1);
  fail_unless(util_frames_size(f1) == 0);
  fail_unless(util_frames_next(f1));
  fail_unless(util_frames_size(f1) == -1);
  

  return 0;
}

