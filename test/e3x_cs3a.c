#include "e3x.h"
#include "util.h"
#include "unit_test.h"
#include "util_sys.h"

// fixtures
#define A_KEY "zgekyeqcjrgnat2dlmqn36fnucz6vk4uzz3funoi7jsfawthpfga";
#define A_SEC "krxnha4pbst2jhnps4vxiybrb6d5onn4mvufg6c766kgsd5eudnq";
#define B_KEY "cufysukisshi5bpbsoshkbdx6s5k6qakakyfona6hfaxio3egjta";
#define B_SEC "3wbj6fd5wb2bljdqi4bwklcc5m5y2vrotk25ee4rywy37ngopffa";

int main(int argc, char **argv)
{
  lob_t opts = lob_new();
  fail_unless(e3x_init(opts) == 0);
  fail_unless(!e3x_err());

  e3x_cipher_t cs = e3x_cipher_set(0x3a,NULL);
  if(!cs) return 0;

  cs = e3x_cipher_set(0,"3a");
  fail_unless(cs);
  fail_unless(cs->id == CS_3a);
  
  uint8_t buf[32];
  fail_unless(e3x_rand(buf,32));

  char hex[65];
  util_hex(e3x_hash((uint8_t*)"foo",3,buf),32,hex);
  fail_unless(strcmp(hex,"2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae") == 0);

  lob_t secrets = e3x_generate();
  fail_unless(secrets);
  fail_unless(lob_get(secrets,"3a"));
  lob_t keys = lob_linked(secrets);
  fail_unless(keys);
  fail_unless(lob_get(keys,"3a"));
  LOG("generated key %s secret %s",lob_get(keys,"3a"),lob_get(secrets,"3a"));

  local_t localA = cs->local_new(keys,secrets);
  fail_unless(localA);

  remote_t remoteA = cs->remote_new(lob_get_base32(keys,"3a"), NULL);
  fail_unless(remoteA);

  // create another to start testing real packets
  lob_t secretsB = e3x_generate();
  fail_unless(lob_linked(secretsB));
  local_t localB = cs->local_new(lob_linked(secretsB),secretsB);
  fail_unless(localB);
  remote_t remoteB = cs->remote_new(lob_get_base32(lob_linked(secretsB),"3a"), NULL);
  fail_unless(remoteB);

  // generate a message
  lob_t messageAB = lob_new();
  lob_set_int(messageAB,"a",42);
  lob_t outerAB = cs->remote_encrypt(remoteB,localA,messageAB);
  fail_unless(outerAB);
  fail_unless(lob_len(outerAB) == 101);

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
  fail_unless(lob_len(couterBA) == 74);

  lob_t outerBA = cs->remote_encrypt(remoteA,localB,messageAB);
  fail_unless(outerBA);
  ephemeral_t ephemAB = cs->ephemeral_new(remoteB,outerBA);
  fail_unless(ephemAB);

  lob_t cinnerAB = cs->ephemeral_decrypt(ephemAB,couterBA);
  fail_unless(cinnerAB);
  fail_unless(util_cmp(lob_get(cinnerAB,"type"),"foo") == 0);

  return 0;
}

