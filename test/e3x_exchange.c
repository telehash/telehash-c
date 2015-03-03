#include "e3x.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);

  // create A
  lob_t idA = e3x_generate();
  fail_unless(idA);
  printf("idA key %.*s secret %.*s\n",(int)lob_linked(idA)->head_len,lob_linked(idA)->head,(int)idA->head_len,idA->head);
  e3x_self_t selfA = e3x_self_new(idA,NULL);
  fail_unless(selfA);
  char *cshex = lob_get_index(idA,0);
  fail_unless(cshex);
  uint8_t csid = 0;
  util_unhex(cshex,2,&csid);
  fail_unless(csid);
  lob_t keyA = lob_get_base32(lob_linked(idA),cshex);
  fail_unless(keyA);

  // create B and an exchange A->B
  lob_t idB = e3x_generate();
  e3x_self_t selfB = e3x_self_new(idB,NULL);
  lob_t keyB = lob_get_base32(lob_linked(idB),cshex);
  e3x_exchange_t xAB = e3x_exchange_new(selfA, csid, keyB);
  fail_unless(xAB);
  fail_unless(xAB->csid == csid);
  fail_unless(e3x_exchange_out(xAB,1));
  fail_unless(xAB->order == xAB->out);

  // create message from A->B
  lob_t innerAB = lob_new();
  lob_set(innerAB,"te","st");
  lob_t msgAB = e3x_exchange_message(xAB, innerAB);
  fail_unless(msgAB);
  fail_unless(msgAB->head_len == 1);
  fail_unless(msgAB->head[0] == csid);
  fail_unless(msgAB->body_len >= 42);

  // decrypt
  lob_t innerAB2 = e3x_self_decrypt(selfB,msgAB);
  fail_unless(innerAB2);
  fail_unless(util_cmp(lob_get(innerAB,"te"),"st") == 0);

  // create exchange B->A and verify message
  e3x_exchange_t xBA = e3x_exchange_new(selfB, csid, keyA);
  fail_unless(xBA);
  fail_unless(e3x_exchange_out(xBA,1));
  fail_unless(e3x_exchange_verify(xBA,msgAB) == 0);

  // generate handshake
  fail_unless(e3x_exchange_out(xAB,3));
  lob_t hsAB = e3x_exchange_handshake(xAB, NULL);
  fail_unless(hsAB);
  fail_unless(hsAB->head_len == 1);
  fail_unless(hsAB->head[0] == csid);
  fail_unless(hsAB->body_len >= 60);

  // sync w/ handshake both ways
  lob_t inAB = e3x_self_decrypt(selfB,hsAB);
  fail_unless(inAB);
  fail_unless(e3x_exchange_verify(xBA,hsAB) == 0);
  fail_unless(e3x_exchange_sync(xBA,hsAB));
  lob_t hsBA = e3x_exchange_handshake(xBA, NULL);
  lob_t inBA = e3x_self_decrypt(selfA,hsBA);
  fail_unless(inBA);
  fail_unless(e3x_exchange_verify(xAB,hsBA) == 0);
  fail_unless(e3x_exchange_sync(xAB,hsBA));
  
  // send/receive channel packet
  lob_t chanAB = lob_new();
  lob_set_int(chanAB,"c",e3x_exchange_cid(xAB, NULL));
  fail_unless(e3x_exchange_cid(xBA, chanAB)); // verify incoming
  lob_t coutAB = e3x_exchange_send(xAB,chanAB);
  fail_unless(coutAB);
  fail_unless(coutAB->body_len >= 33);
  lob_t cinAB = e3x_exchange_receive(xBA,coutAB);
  fail_unless(cinAB);
  fail_unless(lob_get_int(cinAB,"c") == lob_get_int(chanAB,"c"));

  return 0;
}

