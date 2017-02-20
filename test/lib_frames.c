#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  util_frames_t frames;

  frames = util_frames_new(42,1024);
  fail_unless(frames);
  fail_unless(util_frames_inlen(frames) == 0);
  fail_unless(util_frames_outlen(frames) == 0);
  fail_unless(!util_frames_busy(frames));
  fail_unless(!util_frames_free(frames));

  // try a test packet
  frames = util_frames_new(42, 1024);
  lob_t packet = lob_new();
  lob_body(packet,0,100);
  fail_unless(util_frames_send(frames, lob_copy(packet)));
  fail_unless(util_frames_outlen(frames) == 102);
  fail_unless(util_frames_busy(frames));

  // check the header frame
  uint32_t len = 0;
  uint8_t *frame = util_frames_outbox(frames,&len);
  fail_unless(len == 8);
  fail_unless(frame);
  printf("frame %s\n",util_hex(frame,8,NULL));
  fail_unless(strcmp("2a00000066000000", util_hex(frame, 8, NULL)) == 0);

  // receive the header frame
  util_frames_t frames2 = util_frames_new(42, 1024);
  fail_unless(util_frames_inbox(frames2, frame, 8));
  fail_unless(util_frames_sent(frames));

  // expecting data frame now
  uint32_t len2 = 0;
  uint8_t *frame2 = util_frames_awaiting(frames2, &len2);
  fail_unless(frame2);
  fail_unless(len2 == 102);

  // send the data frame
  frame = util_frames_outbox(frames,&len);
  fail_unless(frame);
  fail_unless(len == len2);
  memcpy(frame2,frame,len);
  fail_unless(!util_frames_sent(frames));
  fail_unless(!util_frames_outlen(frames));
  fail_unless(!util_frames_outbox(frames,&len));
  
  // receive the data frame
  fail_unless(util_frames_inbox(frames2,frame2,len2));
  fail_unless(util_frames_inlen(frames2) == 102);
  lob_t packet2 = util_frames_receive(frames2);
  fail_unless(packet2);
  fail_unless(lob_cmp(packet,packet2) == 0);
  fail_unless(!util_frames_receive(frames2));
  fail_unless(util_frames_inlen(frames2) == 0);

  fail_unless(!util_frames_busy(frames));
  fail_unless(!util_frames_free(frames));
  fail_unless(!util_frames_busy(frames2));
  fail_unless(!util_frames_free(frames2));

  util_frames_t fa = util_frames_new(42,1024);
  util_frames_t fb = util_frames_new(42,1024);
  lob_t msg = lob_new();
  lob_body(msg, NULL, 1000);
  for(uint8_t i=0;i<100;i++) util_frames_send(fa,lob_copy(msg));
  uint32_t size = util_frames_outlen(fa);
  LOG_DEBUG("size %lu",size);
  fail_unless(size == 1002 * 100);

  // chat
  while(util_frames_busy(fa))
  {
    util_frames_inbox(fb,util_frames_outbox(fa,&len),len);
    util_frames_sent(fa);
  }

  fail_unless(util_frames_inlen(fb) == size);
  lob_t msg2 = util_frames_receive(fb);
  fail_unless(msg2);
  fail_unless(msg2->body_len == 1000);

  fail_unless(!util_frames_free(fa));
  fail_unless(!util_frames_free(fb));
  
  util_frames_t fuzz = util_frames_new(42,1024);
  uint32_t loops = 10000;
  uint8_t fframe[64];
  e3x_init(NULL); // seed rand
  util_sys_logging(0);
  while(--loops)
  {
    e3x_rand(fframe,64);
    if(util_frames_inbox(fuzz,fframe,64))
    {
      printf("frames accepted bad data: %s\n",util_hex(fframe,64,NULL));
      break;
    }
  }
  fail_unless(loops == 0);
  fail_unless(!util_frames_inlen(fuzz));

  return 0;
}

