
#include <AESLib.h>
#include <cs1a/cs1a.h>
#include <telehash.h>
#include <telehash/net_serial.h>

#define sp Serial.print
#define speol Serial.println

#include <stdarg.h>
void *util_sys_log(const char *file, int line, const char *function, const char * format, ...)
{
    char buffer[256];
    va_list args;
    va_start (args, format);
    vsnprintf (buffer, 256, format, args);
    speol(buffer);
    va_end (args);
    return NULL;
}


int RNG(uint8_t *p_dest, unsigned p_size)
{
  e3x_rand(p_dest, (size_t)p_size);
  return 1;
}


static uint8_t status = 0;

void pong(link_t link, lob_t ping, void *arg)
{
  LOG("pong'd %s",lob_json(ping));
  status = 1;
}

// ping as soon as the link is up
void link_check(link_t link)
{
  if(link_up(link))
  {
    LOG("link is up, pinging");
//    path_ping(link,pong,NULL);
  }
}

mesh_t mesh;
net_serial_t net;

void setup() {
  lob_t keys, secrets, options, json;
  hashname_t id;

  randomSeed(analogRead(0));
  Serial.begin(115200);
  speol("init");

  options = lob_new();
  if(e3x_init(options))
  {
    LOG("e3x init failed: %s",lob_get(options,"err"));
    return;
  }
  
  LOG("*** generating mesh ***");

  mesh = mesh_new(11);
  mesh_generate(mesh);
  mesh_on_discover(mesh,"auto",mesh_add); // accept anyone
  mesh_on_link(mesh, "test", link_check); // testing the event being triggered
  status = 0;
  net = net_serial_new(mesh, NULL);

  LOG("%s",lob_json(mesh_json(mesh)));
}


void loop() {
}

