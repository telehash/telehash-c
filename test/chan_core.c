#include "e3x.h"
#include "util_sys.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);

  // test creation validation
  lob_t open = lob_new();
  chan_t chan = chan_new(NULL);
  fail_unless(!chan);
  chan = chan_new(open);
  fail_unless(!chan);
  lob_set_int(open,"c",1);
  chan = chan_new(open);
  fail_unless(!chan);

  // actually create a channel now
  lob_set(open,"type","test");
  chan = chan_new(open);
  fail_unless(chan);
  fail_unless(chan_state(chan) == CHAN_OPENING);
  fail_unless(chan_id(chan) == 1);
  fail_unless(chan_size(chan,0) == 0);
  fail_unless(chan_uid(chan) && strlen(chan_uid(chan)) == 8);
  fail_unless(chan_c(chan) && strlen(chan_c(chan)) == 1);
  fail_unless(util_cmp(lob_get(chan_open(chan),"type"),"test") == 0);
  
  // test timeout erroring
  fail_unless(chan_timeout(chan,1) == 1);
  fail_unless(chan_receive(chan,NULL,2) == 0);
  lob_t err = chan_receiving(chan);
  fail_unless(err);
  fail_unless(lob_get(err,"err"));

  // receive packets
  lob_t incoming = lob_new();
  lob_set_int(incoming,"test",42);
  fail_unless(chan_receive(chan,incoming,0) == 0);
  fail_unless(lob_get_int(chan_receiving(chan),"test") == 42);
  fail_unless(chan_receiving(chan) == NULL);

  // send packets
  lob_t outgoing = chan_packet(chan);
  fail_unless(lob_get_int(outgoing,"c") == 1);
  lob_set_int(outgoing,"test",42);
  fail_unless(chan_send(chan,outgoing) == 0);
  fail_unless(lob_get_int(chan_sending(chan,0),"test") == 42);
  fail_unless(chan_sending(chan,0) == NULL);

  // create a reliable channel
  chan_free(chan);
  lob_set(open,"type","test");
  lob_set_int(open,"seq",1);
  chan = chan_new(open);
  fail_unless(chan);
  fail_unless(chan_state(chan) == CHAN_OPENING);
  fail_unless(chan_sending(chan,0) == NULL);
  
  // receive an out of order packet
  lob_t ooo = lob_new();
  lob_set_int(ooo,"seq",10);
  fail_unless(chan_receive(chan, lob_copy(ooo),0) == 0);
  fail_unless(chan_receive(chan, ooo,0) == 1); // dedup drop
  fail_unless(chan_receiving(chan) == NULL); // nothing to receive yet
  lob_t oob = chan_sending(chan,0); // should have triggered an ack/miss
  printf("OOB %s\n",lob_json(oob));
  fail_unless(oob);
  fail_unless(lob_get_int(oob,"c") == 1);
  fail_unless(util_cmp(lob_get(oob,"ack"),"0") == 0);
  fail_unless(util_cmp(lob_get(oob,"miss"),"[1,1,1,1,1,1,1,1,1,100]") == 0);

  return 0;
}

