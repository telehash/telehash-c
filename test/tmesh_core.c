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
  fail_unless(c->pipe->path);
  LOG("netA %.*s",c->pipe->path->head_len,c->pipe->path->head);


  fail_unless(!tmesh_join(netA, "azdhpa5r", NULL));
  fail_unless((c = tmesh_join(netA, "azdhpa5n", "")));
  mote_t m = tmesh_link(netA, c, link);
  fail_unless(m);
  fail_unless(m->link == link);
  fail_unless(m == tmesh_link(netA, c, link));
  fail_unless(m->ping);
  fail_unless(m->order == 0);
  fail_unless(!m->tx);
  char hex[256];
  LOG("nonce %s",util_hex(m->nonce,8,hex));
  fail_unless(util_cmp(hex,"0000000000000000") == 0);
  LOG("secret %s",util_hex(m->secret,32,hex));
  fail_unless(util_cmp(hex,"e5667e86ecb564f4f04e2b665348381c06765e6f9fa8161d114d5d8046948532") == 0);
  
  m->at = 1;
  knock_t knock = malloc(sizeof(struct knock_struct));
  fail_unless(mote_knock(m,knock,2));
  fail_unless(knock->tx);
  LOG("next is %ld",knock->start);
  fail_unless(knock->start == 134217729);
  fail_unless(mote_knock(m,knock,knock->start+1));
  fail_unless(!knock->tx);
  LOG("next is %ld",knock->start);
  fail_unless(knock->start == 671088641);

  mote_reset(m);
  m->at = 1;
  fail_unless(tmesh_knock(netA,knock,2,dev));
  fail_unless(knock->mote == m);
  LOG("start %ld stop %ld chan %d",knock->start,knock->stop,knock->chan);
  fail_unless(knock->start == 134217729);

  return 0;
}

