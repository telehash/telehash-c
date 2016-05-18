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
  
  // test timeout erroring
  fail_unless(chan_timeout(chan,1) == 1);
  fail_unless(chan_process(chan,2));
  lob_t err = chan_receiving(chan);
  fail_unless(err);
  fail_unless(lob_get(err,"err"));

  // receive packets
  lob_t incoming = lob_new();
  lob_set_int(incoming,"test",42);
  fail_unless(chan_receive(chan,incoming));
  fail_unless(lob_get_int(chan_receiving(chan),"test") == 42);
  fail_unless(chan_receiving(chan) == NULL);

  // send packets
  lob_t outgoing = chan_packet(chan);
  fail_unless(lob_get_int(outgoing,"c") == 1);
  lob_set_int(outgoing,"test",42);
  fail_unless(!chan_send(chan,outgoing)); // dropped, no link

  return 0;
}

