#include "telehash.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  hashname_t hn;
  
  fail_unless(e3x_init(NULL) == 0);

  hn = hashname_str("uvabrvfqacyvgcu8kbrrmk9apjbvgvn2wjechqr3vf9c1zm3hv7g");
  fail_unless(!hn);

  hn = hashname_str("jvdoio6kjvf3yqnxfvck43twaibbg4pmb7y3mqnvxafb26rqllwa");
  fail_unless(hn);
  fail_unless(strlen(hn->hashname) == 52);
  hashname_free(hn);

  // create intermediate fixture
  lob_t im = lob_new();
  lob_set(im,"1a","ym7p66flpzyncnwkzxv2qk5dtosgnnstgfhw6xj2wvbvm7oz5oaq");
  lob_set(im,"3a","bmxelsxgecormqjlnati6chxqua7wzipxliw5le35ifwxlge2zva");
  hn = hashname_key(im, 0);
  fail_unless(hn);
  fail_unless(util_cmp(hn->hashname,"jvdoio6kjvf3yqnxfvck43twaibbg4pmb7y3mqnvxafb26rqllwa") == 0);

  lob_t keys = lob_new();
  lob_set(keys,"1a","vgjz3yjb6cevxjomdleilmzasbj6lcc7");
  lob_set(keys,"3a","hp6yglmmqwcbw5hno37uauh6fn6dx5oj7s5vtapaifrur2jv6zha");
  hn = hashname_keys(keys);
  fail_unless(hn);
  fail_unless(util_cmp(hn->hashname,"jvdoio6kjvf3yqnxfvck43twaibbg4pmb7y3mqnvxafb26rqllwa") == 0);

  fail_unless(hashname_id(NULL,NULL) == 0);
  fail_unless(hashname_id(im,keys) == 0x3a);
  lob_t test = lob_new();
  lob_set(test,"1a","test");
  lob_set(test,"2a","test");
  fail_unless(hashname_id(keys,test) == 0x1a);
  return 0;
}

