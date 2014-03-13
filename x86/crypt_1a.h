#ifndef _CRYPT_1A_H_
#define _CRYPT_1A_H_

int RNG(uint8_t *p_dest,unsigned p_size);
int crypt_init_1a();
int crypt_new_1a(crypt_t c,unsigned char *key,int len);
void crypt_free_1a(crypt_t c);
int crypt_keygen_1a(packet_t p);
int crypt_private_1a(crypt_t c,unsigned char *key,int len);
packet_t crypt_lineize_1a(crypt_t c,packet_t p);
packet_t crypt_delineize_1a(crypt_t c,packet_t p);
int crypt_line_1a(crypt_t c,packet_t inner);
packet_t crypt_openize_1a(crypt_t self,crypt_t c,packet_t inner);
packet_t crypt_deopenize_1a(crypt_t self,packet_t open);
#define INTERFACE 0
#define EXPORT_INTERFACE 0
#define LOCAL_INTERFACE 0
#define EXPORT
#define LOCAL static
#define PUBLIC
#define PRIVATE
#define PROTECTED

#endif /* _CRYPT_1A_H_ */
