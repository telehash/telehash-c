#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  util_frames_t frames;

  frames = util_frames_new(16);
  fail_unless(frames);
  fail_unless(util_frames_inlen(frames) == 0);
  fail_unless(util_frames_outlen(frames) == 0);
  fail_unless(!util_frames_inbox(frames,NULL));
  fail_unless(!util_frames_outbox(frames,NULL));
  fail_unless(!util_frames_free(frames));

  frames = util_frames_new(16);
  lob_t packet = lob_new();
  lob_body(packet,0,100);
  fail_unless(util_frames_send(frames, lob_copy(packet)));
  fail_unless(util_frames_outlen(frames) == 102);


  

  return 0;
}

