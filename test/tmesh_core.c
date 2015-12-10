#include "tmesh.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

// fixtures
#define A_KEY "anfpjrveyyloypswpqzlfkjpwynahohffy"
#define A_SEC "cgcsbs7yphotlb5fxls5ogy2lrc7yxbg"
#define B_KEY "amhofcnwgmolf3owg2kipr5vus7uifydsy"
#define B_SEC "ge4i7h3jln4kltngwftg2yqtjjvemerw"

#include "./tmesh_device.c"
extern struct radio_struct test_device;

int main(int argc, char **argv)
{
  radio_t dev;
  fail_unless(!e3x_init(NULL)); // random seed
  fail_unless((dev = radio_device(&test_device)));
  
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t keyA = lob_new();
  lob_set(keyA,"1a",A_KEY);
  lob_t secA = lob_new();
  lob_set(secA,"1a",A_SEC);
  fail_unless(!mesh_load(meshA,secA,keyA));

  lob_t keyB = lob_new();
  lob_set(keyB,"1a",B_KEY);
  hashname_t hnB = hashname_keys(keyB);
  fail_unless(hnB);
  link_t link = link_get(meshA,hnB->hashname);
  fail_unless(link);
  
  tmesh_t netA = tmesh_new(meshA, NULL);
  fail_unless(netA);
  cmnty_t c = tmesh_join(netA,"qzjb5f4t","foo");
  fail_unless(c);
  fail_unless(c->medium->bin[0] == 134);
  fail_unless(c->public == NULL);
  fail_unless(c->pipe->path);
  LOG("netA %.*s",c->pipe->path->head_len,c->pipe->path->head);
  fail_unless(tmesh_leave(netA,c));

  char hex[256];
  mote_t m;
  fail_unless(!tmesh_join(netA, "azdhpa5r", NULL));
  fail_unless((c = tmesh_join(netA, "azdhpa5n", "")));
  fail_unless(c->public);
  m = c->public;
  memset(m->nonce,42,8); // nonce is random, force stable for fixture testing
  LOG("nonce %s",util_hex(m->nonce,8,hex));
  fail_unless(util_cmp(hex,"2a2a2a2a2a2a2a2a") == 0);

  m = tmesh_link(netA, c, link);
  fail_unless(m);
  fail_unless(m->link == link);
  fail_unless(m == tmesh_link(netA, c, link));
  fail_unless(m->ping);
  fail_unless(m->order == 0);
  memset(m->nonce,3,8); // nonce is random, force stable for fixture testing
  LOG("secret %s",util_hex(m->secret,32,hex));
  fail_unless(util_cmp(hex,"e5667e86ecb564f4f04e2b665348381c06765e6f9fa8161d114d5d8046948532") == 0);
  
  fail_unless(mote_reset(m));
  memset(m->nonce,0,8); // nonce is random, force stable for fixture testing
  knock_t knock = dev->knock;
  fail_unless(mote_advance(m));
  LOG("next is %lld",m->at);
  fail_unless(m->at == 263154);
  fail_unless(mote_knock(m,knock));
  fail_unless(!knock->tx);
  fail_unless(mote_advance(m));
  fail_unless(mote_advance(m));
  fail_unless(mote_knock(m,knock));
  fail_unless(knock->tx);
  LOG("next is %lld",knock->start);
  fail_unless(knock->start == 21302230);

  mote_reset(m);
  memset(m->nonce,2,8); // nonce is random, force stable for fixture testing
  m->at = 10;
  fail_unless(tmesh_process(netA,2));
  fail_unless(m->at == 8);
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d at %lld",knock->tx,knock->start,knock->stop,knock->chan,m->at);
  fail_unless(!knock->tx);
  fail_unless(knock->start == 8);
  fail_unless(knock->stop == 8+1000);
  fail_unless(knock->chan == 30);
//  fail_unless(tmesh_knocked(netA,knock));
  
  fail_unless(mote_advance(m));
  LOG("seek %s",util_hex(m->nonce,8,hex));
  fail_unless(util_cmp(hex,"f16d6a2a93a608b1") == 0);
  fail_unless(mote_advance(m));
  LOG("at is %lu",m->at);
  fail_unless(m->at >= 5816313);

  // public ping now
  m->at = 0xffffff00;
  memset(knock,0,sizeof(struct knock_struct));
  m = c->public;
  LOG("public at is now %lu",m->at);
  fail_unless(tmesh_process(netA,2));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(knock->tx);
  fail_unless(knock->start == 8586228);
  fail_unless(knock->stop == 8587223);
  fail_unless(knock->chan == 14);

  // public ping tx
  memset(m->nonce,4,8); // fixture for testing
  m->order = 1;
  memset(knock,0,sizeof(struct knock_struct));
  fail_unless(tmesh_process(netA,5461193));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(knock->ready);
  fail_unless(knock->tx);
  fail_unless(knock->start == 13809423);
  fail_unless(knock->chan == 14);
  // frame would be random ciphered, but we fixed it to test
  LOG("frame %s",util_hex(knock->frame,32+8,hex)); // just the stable part
  fail_unless(util_cmp(hex,"0404040404040404d0c8499c1f914359b57ff286af3275a61bc0894ad1eb3fec4c6aeda2ca728e25") == 0);
  // let's preted it's an rx now
  knock->tx = 0;
  knock->done = knock->stop; // fake rx good
  LOG("faking rx in");
  fail_unless(tmesh_process(netA,553648170));
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(!knock->done);
  fail_unless(knock->start == 1491757);

  // leave public community
  fail_unless(tmesh_leave(netA,c));

  // two motes meshing
  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  lob_t secB = lob_new();
  lob_set(secB,"1a",B_SEC);
  fail_unless(!mesh_load(meshB,secB,keyB));

  hashname_t hnA = hashname_keys(keyA);
  fail_unless(hnA);
  link_t linkBA = link_get(meshB,hnA->hashname);
  fail_unless(linkBA);
  
  tmesh_t netB = tmesh_new(meshB, NULL);
  fail_unless(netB);
  cmnty_t cB = tmesh_join(netB,"qzjb5f4t","test");
  fail_unless(cB);
  fail_unless(cB->pipe->path);
  LOG("netB %s",lob_json(cB->pipe->path));

  cmnty_t cA = tmesh_join(netA,"qzjb5f4t","test");
  fail_unless(cA);
  fail_unless(cA->pipe->path);
  LOG("netA %s",lob_json(cA->pipe->path));
  
  mote_t mBA = tmesh_link(netB, cB, linkBA);
  fail_unless(mBA);
  fail_unless(mBA->ping);
  fail_unless(mBA->order == 1);
  LOG("secret %s",util_hex(mBA->secret,32,hex));
  fail_unless(util_cmp(hex,"9a972d28dcc211d43eafdca7877bed1bbeaec30fd3740f4b787355d10423ad12") == 0);

  mote_t mAB = tmesh_link(netA, cA, link);
  fail_unless(mAB);
  fail_unless(mAB->ping);
  fail_unless(mAB->order == 0);
  LOG("secret %s",util_hex(mAB->secret,32,hex));
  fail_unless(util_cmp(hex,"9a972d28dcc211d43eafdca7877bed1bbeaec30fd3740f4b787355d10423ad12") == 0);
  
  knock_t knAB = dev->knock;
  memset(knAB,0,sizeof(struct knock_struct));
  memset(mAB->nonce,12,8);
  fail_unless(tmesh_process(netA,1));
  fail_unless(knAB->mote == mAB);
  LOG("AB tx is %d chan %d at %lu nonce %s",knAB->tx,knAB->chan,knAB->start,util_hex(mAB->nonce,8,NULL));
  fail_unless(knAB->chan == 35);
  fail_unless(knAB->tx == 0);

  knock_t knBA = malloc(sizeof(struct knock_struct));
  memset(knBA,0,sizeof(struct knock_struct));
  dev->knock = knBA; // manually swapping
  memset(mBA->nonce,0,8);
  fail_unless(tmesh_process(netB,21102591));
  fail_unless(knBA->mote == mBA);
  LOG("BA tx is %d chan %d at %lu",knBA->tx,knBA->chan,knAB->start);
  fail_unless(knBA->chan == 35);
  fail_unless(knBA->tx == 1);

  // fake reception, with fake cake
  memcpy(knAB->frame,knBA->frame,64);
  knAB->done = knAB->stop;
  knBA->done = knBA->stop;
  
  LOG("process netA");
  dev->knock = knAB; // manually swapping
  fail_unless(tmesh_process(netA,knAB->done+1));
  fail_unless(mAB->pong);

  LOG("process netB");
  dev->knock = knBA; // manually swapping
  fail_unless(tmesh_process(netB,knBA->done+1));

  // back to the future
  dev->knock = knAB; // manually swapping
  while(knAB->mote != mAB) fail_unless(tmesh_process(netA,mAB->at+1));
  LOG("AB tx is %d chan %d at %lu nonce %s",knAB->tx,knAB->chan,knAB->start,util_hex(mAB->nonce,8,NULL));
  fail_unless(knAB->tx == 1);
  dev->knock = knBA; // manually swapping
  while(knBA->mote != mBA) fail_unless(tmesh_process(netB,mBA->at+1));
  LOG("BA tx is %d chan %d at %lu nonce %s",knBA->tx,knBA->chan,knAB->start,util_hex(mBA->nonce,8,NULL));
  fail_unless(knBA->tx == 0);
  
  // dance
  LOG("transceive");
  memcpy(knBA->frame,knAB->frame,64);
  knAB->done = knAB->stop;
  knBA->done = knBA->stop;

  LOG("BA ping %d pong %d",mBA->ping,mBA->pong);
  LOG("AB ping %d pong %d",mAB->ping,mAB->pong);

  LOG("process netB");
  dev->knock = knBA; // manually swapping
  fail_unless(tmesh_process(netB,1));

  LOG("BA ping %d pong %d",mBA->ping,mBA->pong);
  LOG("AB ping %d pong %d",mAB->ping,mAB->pong);

  // in sync!
  fail_unless(!mBA->pong);
  fail_unless(!mBA->ping);
  
  LOG("process netA");
  dev->knock = knAB; // manually swapping
  fail_unless(tmesh_process(netA,1));

  LOG("BA ping %d pong %d",mBA->ping,mBA->pong);
  LOG("AB ping %d pong %d",mAB->ping,mAB->pong);

  // in sync!
  fail_unless(!mAB->pong);
  fail_unless(!mAB->ping);

  // continue establishing link
  uint8_t max = 10;
  uint32_t step;
  mBA->at = mAB->at;
  memcpy(mBA->nonce,mAB->nonce,8);
  memset(knBA,0,sizeof(struct knock_struct));
  while(--max && !link_up(mBA->link) && !link_up(mAB->link))
  {
    step = mBA->at+1;
    dev->knock = knBA;
    if(tmesh_process(netA,step)) knAB->done = 1;
    dev->knock = knAB;
    if(tmesh_process(netB,step)) knBA->done = 1;
  }
  LOG("linked by %d",max);
  // TODO, quite broken yet, faux driver logic above is way off
//  fail_unless(max);

  return 0;
}

