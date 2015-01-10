#include "socketio.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  // TODO stub only
  fail_unless(socketio_decode(NULL) == NULL);
  fail_unless(socketio_encode(0, 0, NULL) == NULL);

  return 0;
}

