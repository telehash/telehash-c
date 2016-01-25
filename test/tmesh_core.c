#include "tmesh.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

// fixtures
#define A_KEY "anfpjrveyyloypswpqzlfkjpwynahohffy"
#define A_SEC "cgcsbs7yphotlb5fxls5ogy2lrc7yxbg"
#define B_KEY "amhofcnwgmolf3owg2kipr5vus7uifydsy"
#define B_SEC "ge4i7h3jln4kltngwftg2yqtjjvemerw"

tmesh_t netA = NULL, netB = NULL;
#define RXTX(a,b) (a->tx)?memcpy(b->frame,a->frame,64):memcpy(a->frame,b->frame,64)

#include "./tmesh_device.c"

int main(int argc, char **argv)
{
  radio_t devA, devB;
  fail_unless(!e3x_init(NULL)); // random seed
  fail_unless((devA = radio_device(&radioA)));
  fail_unless((devB = radio_device(&radioB)));
  
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t keyA = lob_new();
  lob_set(keyA,"1a",A_KEY);
  lob_t secA = lob_new();
  lob_set(secA,"1a",A_SEC);
  fail_unless(!mesh_load(meshA,secA,keyA));
  mesh_on_discover(meshA,"auto",mesh_add);

  lob_t keyB = lob_new();
  lob_set(keyB,"1a",B_KEY);
  hashname_t hnB = hashname_vkeys(keyB);
  fail_unless(hnB);
  link_t link = link_get(meshA,hnB);
  fail_unless(link);
  
  netA = tmesh_new(meshA, NULL);
  fail_unless(netA);
  cmnty_t c = tmesh_join(netA,"qzjb5f4t","foo");
  fail_unless(c);
  fail_unless(c->medium->bin[0] == 134);
  fail_unless(c->medium->radio == devA->id);
  fail_unless(c->beacons == NULL);
  fail_unless(c->pipe->path);
  LOG("netA %.*s",c->pipe->path->head_len,c->pipe->path->head);
  fail_unless(tmesh_leave(netA,c));

  char hex[256];
  netA->last = 1;
  fail_unless(!tmesh_join(netA, "azdhpa5r", NULL));
  fail_unless((c = tmesh_join(netA, "azdhpa5n", "Public")));
  fail_unless(c->beacons);
  fail_unless(c->beacons->public);
  mote_t m = c->beacons;
  mote_t mpub = m;
  memset(m->nonce,42,8); // nonce is random, force stable for fixture testing
  LOG("nonce %s",util_hex(m->nonce,8,hex));
  fail_unless(util_cmp(hex,"2a2a2a2a2a2a2a2a") == 0);
  LOG("public at is now %lu",c->beacons->at);

  m = tmesh_link(netA, c, link);
  fail_unless(m);
  fail_unless(m->link == link);
  fail_unless(m == tmesh_link(netA, c, link));
  fail_unless(m->order == 0);
  memset(m->nonce,3,8); // nonce is random, force stable for fixture testing
  LOG("secret %s",util_hex(m->secret,32,hex));
  fail_unless(util_cmp(hex,"b7bc9e4f1f128f49a3bcef321450b996600987b129723cc7ae752d6500883c65") == 0);
  LOG("public at is now %lu",mpub->at);
  
  fail_unless(mote_reset(m));
  memset(m->nonce,0,8); // nonce is random, force stable for fixture testing
  knock_t knock = devA->knock;
  fail_unless(mote_advance(m));
  LOG("next is %lld",m->at);
  fail_unless(m->at == 7379);
  fail_unless(mote_knock(m,knock));
  fail_unless(!knock->tx);
  fail_unless(mote_advance(m));
  fail_unless(mote_advance(m));
  fail_unless(mote_advance(m));
  fail_unless(mote_advance(m));
  fail_unless(mote_knock(m,knock));
  fail_unless(knock->tx);
  LOG("next is %lld",knock->start);
  fail_unless(knock->start == 24955);
  LOG("public at is now %lu",mpub->at);
  fail_unless(mpub->at == 1);
  mpub->at = 5;

  mote_reset(m);
  memset(m->nonce,2,8); // nonce is random, force stable for fixture testing
  m->at = 10;
  fail_unless(tmesh_process(netA,2,2));
  fail_unless(m->at == 8);
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d at %lld",knock->tx,knock->start,knock->stop,knock->chan,m->at);
  fail_unless(!knock->tx);
  fail_unless(knock->start == 8);
  fail_unless(knock->stop == 8+10);
  fail_unless(knock->chan == 11);
//  fail_unless(tmesh_knocked(netA,knock));
  LOG("public at is now %lu",c->beacons->at);
  
  fail_unless(mote_advance(m));
  LOG("seek %s",util_hex(m->nonce,8,hex));
  fail_unless(util_cmp(hex,"dc09a8ca7f5cb75e") == 0);
  fail_unless(mote_advance(m));
  LOG("at is %lu",m->at);
  fail_unless(m->at >= 11852);

  // public ping now
//  m->at = 0xffffffff;
  memset(knock,0,sizeof(struct knock_struct));
  m = mpub;
  LOG("public at is now %lu",m->at);
  fail_unless(tmesh_process(netA,4,0));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(!knock->tx);
  fail_unless(knock->start == 3);
  fail_unless(knock->stop == 1003);
  fail_unless(knock->chan == 14);

  // public ping tx
  memset(m->nonce,4,8); // fixture for testing
  m->order = 1;
  memset(knock,0,sizeof(struct knock_struct));
  fail_unless(tmesh_process(netA,4242,0));
  fail_unless(knock->mote == m);
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(knock->ready);
  fail_unless(knock->tx);
  fail_unless(knock->start == 8405);
  fail_unless(knock->chan == 14);
  // frame would be random ciphered, but we fixed it to test
  LOG("frame %s",util_hex(knock->frame,32+8,hex)); // just the stable part
  fail_unless(util_cmp(hex,"e7a3c0d30709d7a4d4585b53a37cc19b96a7294a958aebd025a02bd5582202335cfd213db0363eb7") == 0);
  // let's preted it's an rx now
  knock->tx = 0;
  knock->done = knock->stop; // fake rx good
  LOG("faking rx in");
  fail_unless(!tmesh_knocked(netA,knock)); // identity crisis
  fail_unless(tmesh_process(netA,42424,0));
  LOG("tx %d start %lld stop %lld chan %d",knock->tx,knock->start,knock->stop,knock->chan);
  fail_unless(knock->done);
  fail_unless(knock->start == 43392);

  // leave public community
  fail_unless(tmesh_leave(netA,c));
  
  // two motes meshing
  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  lob_t secB = lob_new();
  lob_set(secB,"1a",B_SEC);
  fail_unless(!mesh_load(meshB,secB,keyB));
  mesh_on_discover(meshB,"auto",mesh_add);

  hashname_t hnA = hashname_vkeys(keyA);
  fail_unless(hnA);
  link_t linkBA = link_get(meshB,hnA);
  fail_unless(linkBA);
  
  netB = tmesh_new(meshB, NULL);
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
  fail_unless(mBA->order == 1);
  LOG("mBA %s secret %s",hashname_short(mBA->link->id),util_hex(mBA->secret,32,hex));
  fail_unless(util_cmp(hex,"9a972d28dcc211d43eafdca7877bed1bbeaec30fd3740f4b787355d10423ad12") == 0);

  mote_t mAB = tmesh_link(netA, cA, link);
  fail_unless(mAB);
  fail_unless(mAB->order == 0);
  LOG("mAB %s secret %s",hashname_short(mAB->link->id),util_hex(mAB->secret,32,hex));
  fail_unless(util_cmp(hex,"9a972d28dcc211d43eafdca7877bed1bbeaec30fd3740f4b787355d10423ad12") == 0);
  
  knock_t knBA = devB->knock;
  knBA->ready = 0;
  memset(mBA->nonce,0,8);
  fail_unless(tmesh_process(netB,10,0));
  fail_unless(knBA->mote == mBA);
  LOG("BA tx is %d chan %d at %lu",knBA->tx,knBA->chan,knBA->start);
  fail_unless(knBA->chan == 35);
  fail_unless(knBA->tx == 1);

  knock_t knAB = devA->knock;
  knAB->ready = 0;
  memset(mAB->nonce,11,8);
  fail_unless(tmesh_process(netA,1,0));
  fail_unless(knAB->mote == mAB);
  LOG("AB tx is %d chan %d at %lu nonce %s",knAB->tx,knAB->chan,knAB->start,util_hex(mAB->nonce,8,NULL));
  fail_unless(knAB->chan == 35);
  fail_unless(knAB->tx == 0);

  // fake reception, with fake cake
  LOG("process netA");
  RXTX(knAB,knBA);
  fail_unless(tmesh_knocked(netA,knAB));
  fail_unless(tmesh_process(netA,knAB->done+1,0));
  fail_unless(mAB->ack);

  LOG("process netB");
  RXTX(knAB,knBA);
  fail_unless(tmesh_knocked(netA,knBA));
  fail_unless(tmesh_process(netB,knBA->done+1,0));

  // dummy data for sync send
  netA->pubim = hashname_im(netA->mesh->keys, hashname_id(netA->mesh->keys,netA->mesh->keys));
  netB->pubim = hashname_im(netB->mesh->keys, hashname_id(netB->mesh->keys,netB->mesh->keys));

  // back to the future
  RXTX(knAB,knBA);
  LOG("mAB %lu mBA %lu",mAB->at,mBA->at);
  fail_unless(tmesh_knocked(netB,knBA));
  while(knAB->mote != mAB) fail_unless(tmesh_process(netA,mAB->at+1,0));
  LOG("AB tx is %d chan %d at %lu nonce %s",knAB->tx,knAB->chan,knAB->start,util_hex(mAB->nonce,8,NULL));
  fail_unless(knAB->tx == 1);
  RXTX(knAB,knBA);
  fail_unless(tmesh_knocked(netA,knAB));
  LOG("mAB %lu mBA %lu",mAB->at,mBA->at);
  while(knBA->mote != mBA) fail_unless(tmesh_process(netB,mBA->at+1,0));
  LOG("BA tx is %d chan %d at %lu nonce %s",knBA->tx,knBA->chan,knAB->start,util_hex(mBA->nonce,8,NULL));
  fail_unless(knBA->tx == 0);

  // in sync!
  fail_unless(!mBA->ack);
  fail_unless(!mAB->ack);
  mAB->at = mBA->at;
  LOG("mAB %lu mBA %lu",mAB->at,mBA->at);
  fail_unless(mAB->at == mBA->at);
  
  // continue establishing link
  uint8_t max = 20;
  uint32_t step = mAB->at;
  while(--max && !link_up(mBA->link) && !link_up(mAB->link))
  {
//    printf("\n\n%d %u\n",max,step);

    tmesh_process(netA,step,0);
    tmesh_process(netB,step,0);

    LOG("AB %d %d/%d BA %d %d/%d",knAB->tx,knAB->start,knAB->stop,knBA->tx,knBA->start,knBA->stop);
    step = (knAB->stop < knBA->stop)?knAB->stop:knBA->stop;

    if(knAB->chan == knBA->chan)
    {
      RXTX(knAB,knBA);
      step = knAB->stop = knBA->stop; // must be same
    }

    if(step == knAB->stop) tmesh_knocked(netA,knAB);
    if(step == knBA->stop) tmesh_knocked(netB,knBA);

  }
  LOG("linked by %d",max);
  fail_unless(max);

  return 0;
}

