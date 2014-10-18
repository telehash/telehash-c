#include "hashname.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  hashname_t hn;

  hn = hashname_str("uvabrvfqacyvgcu8kbrrmk9apjbvgvn2wjechqr3vf9c1zm3hv7g");
  fail_unless(!hn);

  hn = hashname_str("jvdoio6kjvf3yqnxfvck43twaibbg4pmb7y3mqnvxafb26rqllwa");
  fail_unless(hn);
  fail_unless(strlen(hn->hashname) == 52);

  return 0;
}

