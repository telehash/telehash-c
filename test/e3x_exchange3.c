#include "e3x.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);

  // create A
  lob_t idA = e3x_generate();
  fail_unless(idA);
  printf("idA key %.*s secret %.*s\n",lob_linked(idA)->head_len,lob_linked(idA)->head,idA->head_len,idA->head);
  self3_t selfA = self3_new(idA,NULL);
  fail_unless(selfA);
  lob_t keyA = lob_get_base32(lob_linked(idA),"1a");
  fail_unless(keyA);

  // create B and an exchange A->B
  lob_t idB = e3x_generate();
  self3_t selfB = self3_new(idB,NULL);
  lob_t keyB = lob_get_base32(lob_linked(idB),"1a");
  exchange3_t xAB = exchange3_new(selfA, 0x1a, keyB, 1);
  fail_unless(xAB);
  fail_unless(xAB->order == xAB->at);
  fail_unless(xAB->csid == 0x1a);

  // create message from A->B
  lob_t innerAB = lob_new();
  lob_set(innerAB,"te","st");
  lob_t msgAB = exchange3_message(xAB, innerAB);
  fail_unless(msgAB);
  fail_unless(msgAB->head_len == 1);
  fail_unless(msgAB->head[0] == 0x1a);
  fail_unless(msgAB->body_len == 42);

  // decrypt
  lob_t innerAB2 = self3_decrypt(selfB,msgAB);
  fail_unless(innerAB2);
  fail_unless(util_cmp(lob_get(innerAB,"te"),"st") == 0);

  // create exchange B->A and verify message
  exchange3_t xBA = exchange3_new(selfB, 0x1a, keyA, 1);
  fail_unless(xBA);
  fail_unless(exchange3_verify(xBA,msgAB) == 0);

  // generate handshake
  lob_t hsAB = exchange3_handshake(xAB, 3);
  fail_unless(hsAB);
  fail_unless(hsAB->head_len == 1);
  fail_unless(hsAB->head[0] == 0x1a);
  fail_unless(hsAB->body_len == 60);

  // sync w/ handshake both ways
  lob_t inAB = self3_decrypt(selfB,hsAB);
  fail_unless(inAB);
  int sync = exchange3_sync(xBA,hsAB,inAB);
  fail_unless(sync);
  lob_t hsBA = exchange3_handshake(xBA, sync);
  lob_t inBA = self3_decrypt(selfA,hsBA);
  fail_unless(inBA);
  sync = exchange3_sync(xAB,hsBA,inBA);
  fail_unless(sync);
  
  // send/receive channel packet
  lob_t chanAB = lob_new();
  lob_set_int(chanAB,"c",exchange3_cid(xAB));
  lob_t coutAB = exchange3_send(xAB,chanAB);
  fail_unless(coutAB);
  fail_unless(coutAB->body_len == 33);
  lob_t cinAB = exchange3_receive(xBA,coutAB);
  fail_unless(cinAB);
  fail_unless(lob_get_int(cinAB,"c") == lob_get_int(chanAB,"c"));

  return 0;
}

