#include "e3x.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);
  lob_t id = e3x_generate();
  fail_unless(id);

  self3_t self = self3_new(id,NULL);
  fail_unless(self);
  fail_unless(self->locals[CS_1a]);
  fail_unless(self->keys[CS_1a]);
  fail_unless(self->keys[CS_1a]->body_len);
  fail_unless(lob_get(self->keys[CS_1a],"key"));
  fail_unless(lob_get(self->keys[CS_1a],"hash"));
  fail_unless(strlen(lob_get(self->keys[CS_1a],"hash")) == 52);

  return 0;
}

