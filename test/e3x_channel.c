#include "e3x.h"
#include "util_sys.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);

  // test creation validation
  lob_t open = lob_new();
  e3x_channel_t chan = e3x_channel_new(NULL);
  fail_unless(!chan);
  chan = e3x_channel_new(open);
  fail_unless(!chan);
  lob_set_int(open,"c",1);
  chan = e3x_channel_new(open);
  fail_unless(!chan);

  // actually create a channel now
  lob_set(open,"type","test");
  chan = e3x_channel_new(open);
  fail_unless(chan);
  fail_unless(e3x_channel_state(chan) == OPENING);
  fail_unless(e3x_channel_id(chan) == 1);
  fail_unless(e3x_channel_size(chan,0) == 0);
  fail_unless(e3x_channel_uid(chan) && strlen(e3x_channel_uid(chan)) == 8);
  fail_unless(e3x_channel_c(chan) && strlen(e3x_channel_c(chan)) == 1);
  fail_unless(util_cmp(lob_get(e3x_channel_open(chan),"type"),"test") == 0);
  
  // set up timeout
  e3x_event_t ev = e3x_event_new(1, util_sys_ms(0));
  fail_unless(e3x_channel_timeout(chan,ev,1) == 1);

  // receive packets
  lob_t incoming = lob_new();
  lob_set_int(incoming,"test",42);
  fail_unless(e3x_channel_receive(chan,incoming) == 0);
  fail_unless(lob_get_int(e3x_channel_receiving(chan),"test") == 42);
  fail_unless(e3x_channel_receiving(chan) == NULL);

  // send packets
  lob_t outgoing = e3x_channel_packet(chan);
  fail_unless(lob_get_int(outgoing,"c") == 1);
  lob_set_int(outgoing,"test",42);
  fail_unless(e3x_channel_send(chan,outgoing) == 0);
  fail_unless(lob_get_int(e3x_channel_sending(chan),"test") == 42);
  fail_unless(e3x_channel_sending(chan) == NULL);

  // create a reliable channel
  e3x_channel_free(chan);
  lob_set(open,"type","test");
  lob_set_int(open,"seq",1);
  chan = e3x_channel_new(open);
  fail_unless(chan);
  fail_unless(e3x_channel_state(chan) == OPENING);
  fail_unless(e3x_channel_sending(chan) == NULL);
  
  // receive an out of order packet
  lob_t ooo = lob_new();
  lob_set_int(ooo,"seq",10);
  fail_unless(e3x_channel_receive(chan, lob_copy(ooo)) == 0);
  fail_unless(e3x_channel_receive(chan, ooo) == 1); // dedup drop
  fail_unless(e3x_channel_receiving(chan) == NULL); // nothing to receive yet
  lob_t oob = e3x_channel_sending(chan); // should have triggered an ack/miss
  printf("OOB %s\n",lob_json(oob));
  fail_unless(oob);
  fail_unless(lob_get_int(oob,"c") == 1);
  fail_unless(util_cmp(lob_get(oob,"ack"),"0") == 0);
  fail_unless(util_cmp(lob_get(oob,"miss"),"[1,1,1,1,1,1,1,1,1,100]") == 0);

  return 0;
}

