#include "jwt.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  // TODO stub only
  fail_unless(jwt_decode(NULL) == NULL);

  return 0;
}

