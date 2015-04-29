#include "net_serial.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t secretsA = mesh_generate(meshA);
  fail_unless(secretsA);

  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  lob_t secretsB = mesh_generate(meshB);
  fail_unless(secretsB);
  
  net_serial_t spA = net_serial_new(meshA, NULL);
//  fail_unless(spA);


  return 0;
}

