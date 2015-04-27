
#include <AESLib.h>
#include <cs1a/cs1a.h>
#include <telehash.h>

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


void setup() {
  lob_t keys, secrets, options, json;
  hashname_t id;

  Serial.begin(115200);
  options = lob_new();
  if(e3x_init(options))
  {
    LOG("e3x init failed: %s",lob_get(options,"err"));
    return;
  }

  LOG("*** generating keys ***");
  secrets = e3x_generate();
  keys = lob_linked(secrets);
  if(!keys)
  {
    LOG("keygen failed: %s",e3x_err());
    return;
  }
  id = hashname_keys(keys);
  json = lob_new();
  lob_set(json,"hashname",id->hashname);
  lob_set_raw(json,"keys",0,(char*)keys->head,keys->head_len);
  lob_set_raw(json,"secrets",0,(char*)secrets->head,secrets->head_len);
  speol(lob_json(json));

}


void loop() {
}

