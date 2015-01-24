#include "e3x.h"
#include "util.h"
#include "unit_test.h"
#include "util_sys.h"

// fixtures
#define A_KEY "anfpjrveyyloypswpqzlfkjpwynahohffy";
#define A_SEC "cgcsbs7yphotlb5fxls5ogy2lrc7yxbg";
#define B_KEY "amhofcnwgmolf3owg2kipr5vus7uifydsy";
#define B_SEC "ge4i7h3jln4kltngwftg2yqtjjvemerw";

int main(int argc, char **argv)
{
  lob_t opts = lob_new();
  fail_unless(e3x_init(opts) == 0);
  fail_unless(!e3x_err());

  // need cs1a support to continue testing
  e3x_cipher_t cs = e3x_cipher_set(0x1a,NULL);
  if(!cs) return 0;

  cs = e3x_cipher_set(0,"1a");
  fail_unless(cs);
  fail_unless(cs->id == CS_1a);
  
  uint8_t buf[32];
  fail_unless(e3x_rand(buf,32));

  char hex[65];
  util_hex(e3x_hash((uint8_t*)"foo",3,buf),32,hex);
  fail_unless(strcmp(hex,"2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae") == 0);

  lob_t secrets = e3x_generate();
  fail_unless(secrets);
  fail_unless(lob_get(secrets,"1a"));
  lob_t keys = lob_linked(secrets);
  fail_unless(keys);
  fail_unless(lob_get(keys,"1a"));
  LOG("generated key %s secret %s",lob_get(keys,"1a"),lob_get(secrets,"1a"));

  local_t localA = cs->local_new(keys,secrets);
  fail_unless(localA);

  remote_t remoteA = cs->remote_new(lob_get_base32(keys,"1a"), NULL);
  fail_unless(remoteA);

  // create another to start testing real packets
  lob_t secretsB = e3x_generate();
  fail_unless(lob_linked(secretsB));
  printf("XX %s\n",lob_json(lob_linked(secretsB)));
  local_t localB = cs->local_new(lob_linked(secretsB),secretsB);
  fail_unless(localB);
  remote_t remoteB = cs->remote_new(lob_get_base32(lob_linked(secretsB),"1a"), NULL);
  fail_unless(remoteB);

  // generate a message
  lob_t messageAB = lob_new();
  lob_set_int(messageAB,"a",42);
  lob_t outerAB = cs->remote_encrypt(remoteB,localA,messageAB);
  fail_unless(outerAB);
  fail_unless(lob_len(outerAB) == 42);

  // decrypt and verify it
  lob_t innerAB = cs->local_decrypt(localB,outerAB);
  fail_unless(innerAB);
  fail_unless(lob_get_int(innerAB,"a") == 42);
  fail_unless(cs->remote_verify(remoteA,localB,outerAB) == 0);

  ephemeral_t ephemBA = cs->ephemeral_new(remoteA,outerAB);
  fail_unless(ephemBA);
  
  lob_t channelBA = lob_new();
  lob_set(channelBA,"type","foo");
  lob_t couterBA = cs->ephemeral_encrypt(ephemBA,channelBA);
  fail_unless(couterBA);
  fail_unless(lob_len(couterBA) == 42);

  lob_t outerBA = cs->remote_encrypt(remoteA,localB,messageAB);
  fail_unless(outerBA);
  ephemeral_t ephemAB = cs->ephemeral_new(remoteB,outerBA);
  fail_unless(ephemAB);

  lob_t cinnerAB = cs->ephemeral_decrypt(ephemAB,couterBA);
  fail_unless(cinnerAB);
  fail_unless(util_cmp(lob_get(cinnerAB,"type"),"foo") == 0);

  return 0;
}

