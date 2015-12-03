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
  
  m->at = 1;
  knock_t knock = &dev->knock;
  fail_unless(mote_bttf(m,4200000));
  LOG("next is %lld",m->at);
  fail_unless(m->at == 4399862);
  fail_unless(mote_knock(m,knock));
  fail_unless(knock->tx);
  fail_unless(mote_bttf(m,4399862+1));
  fail_unless(mote_knock(m,knock));
  fail_unless(!knock->tx);
  LOG("next is %lld",knock->start);
  fail_unless(knock->start == 11570343);

  mote_reset(m);
  memset(m->nonce,2,8); // nonce is random, force stable for fixture testing
  m->at = 1;
  fail_unless(tmesh_process(netA,2));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d at %lld",knock->tx,knock->start,knock->stop,knock->chan,m->at);
  fail_unless(knock->tx);
  fail_unless(knock->start == 2779756);
  fail_unless(knock->stop == 2779756+1000);
  fail_unless(knock->chan == 30);
  fail_unless(m->at == 2779756);
  knock->adjust = 1;
//  fail_unless(tmesh_knocked(netA,knock));
  
  uint8_t nonce[8];
  fail_unless(mote_wait(m,4242424242,1,NULL));
  LOG("seek %s",util_hex(m->nwait,8,hex));
  fail_unless(util_cmp(hex,"15b28afc066a9f8f") == 0);
  memcpy(nonce,m->nwait,8);
  fail_unless(mote_wait(m,4242424242,1,nonce));
  LOG("seek %s",util_hex(nonce,8,hex));
  fail_unless(util_cmp(hex,"15b28afc066a9f8f") == 0);
  fail_unless(mote_bttf(m,4242424243));
  fail_unless(m->waiting == 0);
  fail_unless(memcmp(m->nonce,nonce,8) == 0);
  LOG("at is %lu",m->at);
  fail_unless(m->at >= 16019871);

  // public ping now
  m->at = 424294967; // force way future
  m = c->public;
  fail_unless(tmesh_process(netA,3));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(knock->tx);
  fail_unless(knock->start == 5223477);
  fail_unless(knock->stop == 5223477+1000);
  fail_unless(knock->chan == 14);
  // pretend rx failed
//  fail_unless(tmesh_knocked(netA,knock));
  fail_unless(m->at == knock->start);

  // public ping tx
  fail_unless(tmesh_process(netA,437478935));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(knock->tx);
  fail_unless(knock->start == 1530280);
  fail_unless(knock->chan == 14);
  // frame would be random ciphered, but we fixed it to test
  LOG("frame %s",util_hex(knock->frame,32+8,hex)); // just the stable part
  fail_unless(util_cmp(hex,"0731ffea6a27124b0731ffea6a27124b6ea8a74bc285295d4f4d667c4f30a5266b66abc8e1a45e9b") == 0);
  // let's preted it's an rx now
  knock->tx = 0;
  knock->adjust = 1; // fake rx good
//  fail_unless(tmesh_knocked(netA,knock));
  // frame is deciphered
  LOG("frame %s",util_hex(knock->frame,32+8,hex)); // just the stable part
  fail_unless(memcmp(knock->frame,m->nonce,8) == 0);
  fail_unless(memcmp(knock->frame+8+8,meshA->id->bin,32) == 0);
  fail_unless(util_cmp(hex,"0731ffea6a27124b37a5a43c979db0acfea600b08b84ab402fca3951b20b53c87820013574a5bcff") == 0);

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
  
  knock_t knAB = &dev->knock;
  memset(mAB->nonce,12,8);
  fail_unless(tmesh_process(netA,1));
  fail_unless(knAB->mote == mAB);
  LOG("AB tx is %d chan %d at %lu nonce %s",knAB->tx,knAB->chan,knAB->start,util_hex(mAB->nonce,8,NULL));
  fail_unless(knAB->chan == 35);
  fail_unless(knAB->tx == 0);

  knock_t knBA = &dev->knock;
  memset(mBA->nonce,0,8);
  fail_unless(tmesh_process(netB,1));
  fail_unless(knBA->mote == mBA);
  LOG("BA tx is %d chan %d at %lu",knBA->tx,knBA->chan,knAB->start);
  fail_unless(knBA->chan == 35);
  fail_unless(knBA->tx == 1);

  // fake reception, with fake cake
  memcpy(knAB->frame,knBA->frame,64);
  knAB->adjust = 1;
  knBA->adjust = 1;
//  fail_unless(tmesh_knocked(netA,knAB)); // the rx
  fail_unless(mAB->pong);
  fail_unless(mAB->waiting);
  fail_unless(memcmp(mAB->nonce,mBA->nonce,8) == 0);
  fail_unless(memcmp(mAB->nwait,mBA->nwait,8) == 0);
//  fail_unless(tmesh_knocked(netB,knBA)); // the tx

  // back to the future
  while(knAB->mote != mAB) fail_unless(tmesh_process(netA,mAB->at+1));
  LOG("AB tx is %d chan %d at %lu nonce %s",knAB->tx,knAB->chan,knAB->start,util_hex(mAB->nonce,8,NULL));
  fail_unless(knAB->tx == 1);
  while(knBA->mote != mBA) fail_unless(tmesh_process(netB,mBA->at+1));
  LOG("BA tx is %d chan %d at %lu nonce %s",knBA->tx,knBA->chan,knAB->start,util_hex(mBA->nonce,8,NULL));
  fail_unless(knBA->tx == 0);
  
  // dance
  memcpy(knBA->frame,knAB->frame,64);
  knAB->adjust = 1;
  knBA->adjust = 1;
//  fail_unless(tmesh_knocked(netB,knBA)); // the rx
  // in sync!
  fail_unless(!mBA->pong);
  fail_unless(!mBA->ping);
  
//  fail_unless(tmesh_knocked(netA,knAB)); // the tx
  // in sync!
  fail_unless(!mAB->pong);
  fail_unless(!mAB->ping);
  

  return 0;
}

