#ifndef path_h
#define path_h

typedef struct path_struct
{
  char type[12];
  unsigned char *json;
} *path_t;

path_t path_new(char *type);
void path_free(path_t p);

#endif