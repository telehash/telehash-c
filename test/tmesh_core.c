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
  knock_t knock = malloc(sizeof(struct knock_struct));
  fail_unless(mote_knock(m,knock,2));
  fail_unless(knock->tx);
  LOG("next is %lld",knock->start);
  fail_unless(knock->start == 197380);
  fail_unless(mote_knock(m,knock,knock->start+1));
  fail_unless(!knock->tx);
  LOG("next is %lld",knock->start);
  fail_unless(knock->start == 7831333);

  mote_reset(m);
  memset(m->nonce,1,8); // nonce is random, force stable for fixture testing
  m->at = 1;
  fail_unless(tmesh_knock(netA,knock,2,dev));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(knock->tx);
  fail_unless(knock->start == 65794);
  fail_unless(knock->stop == 65794+1000);
  fail_unless(knock->chan == 30);
  fail_unless(m->at == 1);
  knock->actual = knock->start;
  fail_unless(tmesh_knocked(netA,knock));
  fail_unless(m->at == knock->start);
  
  uint8_t nonce[8];
  fail_unless(mote_seek(m,4242424242,1,nonce));
  LOG("seek %s",util_hex(nonce,8,hex));
  fail_unless(util_cmp(hex,"190e24cb0e578019") == 0);
  uint64_t at = mote_seek(m,4242424242,0,nonce);
  LOG("seek %s at %lu",util_hex(nonce,8,hex),(long unsigned int)at);
  fail_unless(at == 4242793192);
  fail_unless(util_cmp(hex,"1b886a6e45451adc") == 0);
  uint8_t ntmp[8];
  memcpy(ntmp,m->nonce,8);
  memcpy(m->nwait,nonce,8);
  m->waiting = 1;
  fail_unless(mote_window(m));
  fail_unless(memcmp(m->nonce,nonce,8) == 0);
  fail_unless(m->at > 4242793192);
  memcpy(m->nonce,ntmp,8); // restore for test fixtures

  // public ping now
  m->at = 424294967296; // force way future
  m = c->public;
  fail_unless(tmesh_knock(netA,knock,3,dev));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(!knock->tx);
  fail_unless(knock->start == 2763307);
  fail_unless(knock->stop == 2763307+1000);
  fail_unless(knock->chan == 14);
  // pretend rx failed
  fail_unless(tmesh_knocked(netA,knock));
  fail_unless(m->at == knock->start);

  // public ping tx
  fail_unless(tmesh_knock(netA,knock,4664066049,dev));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(knock->tx);
  fail_unless(knock->start == 4676829453);
  fail_unless(knock->chan == 14);
  // frame would be random ciphered, but we fixed it to test
  LOG("frame %s",util_hex(knock->frame,32+8,hex)); // just the stable part
  fail_unless(util_cmp(hex,"27d6fcdfbcbac5f5e25477919651172ed7a081ecb1639b80349fccb40117c42c4c65feaa638093e7") == 0);
  // let's preted it's an rx now
  knock->tx = 0;
  knock->actual = knock->start; // fake rx good
  fail_unless(tmesh_knocked(netA,knock));
  fail_unless(m->at == knock->actual);
  // frame is deciphered
  LOG("frame %s",util_hex(knock->frame,32+8,hex)); // just the stable part
  fail_unless(memcmp(knock->frame,m->nonce,8) == 0);
  fail_unless(memcmp(knock->frame+8+8,meshA->id->bin,32) == 0);
  fail_unless(util_cmp(hex,"27d6fcdfbcbac5f58d9856961a90d930fea600b08b84ab402fca3951b20b53c87820013574a5bcff") == 0);

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
  
  knock_t knAB, knBA;
  knAB = malloc(sizeof(struct knock_struct));
  knBA = malloc(sizeof(struct knock_struct));
  fail_unless(tmesh_knock(netA,knAB,3,dev));
  LOG("xxx %d %d %d",knAB->mote,mAB,mBA);
  fail_unless(knAB->mote == mAB);

  return 0;
}

