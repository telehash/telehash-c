#include <tomcrypt.h>
#include <tommath.h>
#include <js0n.h>
#include <j0g.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

char *hex(unsigned char *digest, int len, char *out);
unsigned char *unhex(char *hex, int len, unsigned char *out);
int hashname(unsigned char *key, int klen, char *hn);

int main(int argc, char *argv[])
{
  ecc_key mykey, urkey;
  prng_state prng;
  int err, x, seed_port, inner_len, outer_len, sock, body_len;
  unsigned char buf[4096], ecc_enc[256], rand16[16], aesiv[16], aeskey[32], inner[2048], inner_enc[2048], inner_sig[512], inner_csig[512], csig[512], sighash[32], outer[2048], *body;
  char out[4096], inbuf[32768], *ibat, *pem, self_hn[65], seed_hn[65], seed_ip[32];
  char *pem_pre = "-----BEGIN PUBLIC KEY-----\n", *pem_suf = "\n-----END PUBLIC KEY-----\n";
  char line_out[33], *iat, open[512], iv[33], sig[512], *popen, *piv, *psig, *seed_line;
  unsigned char self_der[1024], self_eccpub[65], seed_eccpub[65], secret[64], *seed_der;
  unsigned long len, self_der_len, seed_der_len, len2;
  unsigned short index[32], iatlen, inlen;
  symmetric_CTR ctr;
  rsa_key self_rsa, seed_rsa;
  FILE *idfile;
  struct	sockaddr_in sad, rad, seed_ad;

  ltc_mp = ltm_desc;
  register_cipher(&aes_desc);  
  register_prng(&yarrow_desc);
  register_hash(&sha256_desc);
  register_hash(&sha1_desc);

  // first read in the rsa keypair id
  if(argc < 2)
  {
    printf("./openize idfile.json\n");
    return -1;
  }
  idfile = fopen(argv[1],"rb");
  if(!idfile)
  {
    printf("can't open %s\n",argv[1]);
    return -1;
  }
  x = fread(inbuf,1,sizeof(inbuf),idfile);
  fclose(idfile);
  if(x < 100)
  {
    printf("Error reading id json, length %d is too short\n", x);
    return -1;
  }
  inbuf[x] = 0;
  
  // parse the json
  j0g(inbuf, index, 32);
  if(!*index || !j0g_val("public",inbuf,index) || !j0g_val("private",inbuf,index))
  {
    printf("Error parsing json, must be valid and have public and private keys: %s\n",inbuf);
    return -1;
  }
  
  // load the keypair
  self_der_len = sizeof(self_der);
  if ((err = base64_decode((unsigned char*)j0g_str("public",inbuf,index), strlen(j0g_str("public",inbuf,index)), self_der, &self_der_len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = rsa_import(self_der, self_der_len, &self_rsa)) != CRYPT_OK) {
    printf("Import public error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = hashname(self_der, self_der_len, self_hn)) != CRYPT_OK) {
    printf("hashname error: %s\n", error_to_string(err));
    return -1;
  }
  len = sizeof(buf);
  if ((err = base64_decode((unsigned char*)j0g_str("private",inbuf,index), strlen(j0g_str("private",inbuf,index)), buf, &len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = rsa_import(buf, len, &self_rsa)) != CRYPT_OK) {
    printf("Import private error: %s\n", error_to_string(err));
    return -1;
  }  
  printf("loaded keys for %s\n",self_hn);

  // load the seeds file
  idfile = fopen("seeds.json","rb");
  if(!idfile)
  {
    printf("missing seeds.json\n");
    return -1;
  }
  x = fread(inbuf,1,sizeof(inbuf),idfile);
  fclose(idfile);
  if(x < 100)
  {
    printf("Error reading seeds json, length %d is too short\n", x);
    return -1;
  }
  inbuf[x] = 0;
  
  // parse the json
  j0g(inbuf, index, 32);
  if(!*index || *inbuf != '[' || inbuf[*index] != '{')
  {
    printf("Error parsing seeds json, must be valid and have an array containing an object: %s\n",inbuf);
    return -1;
  }
  inbuf[index[1]+2] = 0; // null terminate the first object
  ibat = inbuf+*index; // now point to just the object
  j0g(ibat, index, 32); // re-parse the inner object
  if(!*index || !j0g_val("pubkey",ibat,index) || !j0g_val("ip",ibat,index) || !j0g_val("port",ibat,index))
  {
    printf("Error parsing seeds json, must contain a valid object having a public, ip, and port: %s\n",ibat);
    return -1;
  }
  
  // load the seed public key and ip:port
  pem = j0g_str("pubkey",ibat,index);
  if(strstr(pem,pem_pre) == pem) pem += strlen(pem_pre);
  if(strstr(pem,pem_suf)) *(strstr(pem,pem_suf)) = 0;
  // pem should now be just the base64
  len = sizeof(buf);
  if ((err = base64_decode((unsigned char*)pem, strlen(pem), buf, &len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = rsa_import(buf, len, &seed_rsa)) != CRYPT_OK) {
    printf("Import public error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = hashname(buf, len, seed_hn)) != CRYPT_OK) {
    printf("hashname error: %s\n", error_to_string(err));
    return -1;
  }
  strcpy(seed_ip,j0g_str("ip",ibat,index));
  seed_port = atoi(j0g_str("port",ibat,index));
  printf("seed loaded %s at %s:%d\n",seed_hn,seed_ip,seed_port);

  /////////////////
  // create the inner packet

  // random line id
  if ((err = rng_make_prng(128, find_prng("yarrow"), &prng, NULL)) != CRYPT_OK) {
    printf("Error setting up PRNG, %s\n", error_to_string(err)); return -1;
  }
  x = yarrow_read(rand16,16,&prng);
  if (x != 16) {
    printf("Error reading PRNG for line required.\n");
    return -1;
  }
  hex(rand16,16,line_out);

  iat = (char*)inner+2;
  sprintf(iat, "{\"line\":\"%s\",\"at\":%d,\"to\":\"%s\"}",line_out,(int)time(0),seed_hn);
  printf("INNER: %s\n",iat);
  iatlen = htons(strlen(iat));
  memcpy(inner,&iatlen,2);
  inner_len = 2+strlen(iat);
  memcpy(inner+inner_len,self_der,self_der_len);
  inner_len += self_der_len;
  printf("inner packet len %d\n",inner_len);


  // generate the ecc keys
  if ((err = ecc_make_key(&prng, find_prng("yarrow"), 32, &mykey)) != CRYPT_OK) {
    printf("Error making key: %s\n", error_to_string(err));
	return -1;
  }
  len = sizeof(self_eccpub);
  if ((err = ecc_ansi_x963_export(&mykey, self_eccpub, &len)) != CRYPT_OK) {
    printf("Error exporting ecc key: %s\n", error_to_string(err));
	return -1;
  }
  printf("ecc pub key %s\n",hex(self_eccpub,65,out));

  // create the aes cipher key/iv
  len = 32;
  if ((err = hash_memory(find_hash("sha256"),self_eccpub,65,aeskey,&len)) != CRYPT_OK) {
     printf("Error hashing key: %s\n", error_to_string(err));
     return -1;
  }
  x = yarrow_read(aesiv,16,&prng);
  if (x != 16) {
    printf("Error reading PRNG for IV required.\n");
    return -1;
  }
  hex(aesiv,16,iv);
  printf("aes key %s iv %s\n",hex(aeskey,32,out),iv);

  // create aes cipher now and encrypt the inner
  if ((err = ctr_start(find_cipher("aes"),aesiv,aeskey,32,0,CTR_COUNTER_BIG_ENDIAN,&ctr)) != CRYPT_OK) {
     printf("ctr_start error: %s\n",error_to_string(err));
     return -1;
  }
  if ((err = ctr_encrypt(inner,inner_enc,inner_len,&ctr)) != CRYPT_OK) {
     printf("ctr_encrypt error: %s\n", error_to_string(err));
     return -1;
  }
  printf("aes encrypt %s\n",hex(inner_enc,inner_len,out));

  // encrypt the ecc key
  len2 = sizeof(ecc_enc);
  if ((err = rsa_encrypt_key(self_eccpub, 65, ecc_enc, &len2, 0, 0, &prng, find_prng("yarrow"), find_hash("sha1"), &seed_rsa)) != CRYPT_OK) {
    printf("rsa_encrypt error: %s\n", error_to_string(err));
    return -1;
  }
  len = sizeof(open);
  if ((err = base64_encode(ecc_enc, len2, (unsigned char*)open, &len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  open[len] = 0;

  // sign the inner_enc
  len = 32;
  if ((err = hash_memory(find_hash("sha256"),inner_enc,inner_len,sighash,&len)) != CRYPT_OK) {
     printf("Error hashing key: %s\n", error_to_string(err));
     return -1;
  }
  len = sizeof(inner_sig);
  if ((err = rsa_sign_hash_ex(sighash, 32, inner_sig, &len, LTC_PKCS_1_V1_5, &prng, find_prng("yarrow"), find_hash("sha256"), 12, &self_rsa)) != CRYPT_OK) {
    printf("rsa_sign error: %s\n", error_to_string(err));
    return -1;
  }
	// encrypt the signature
  if ((err = ctr_encrypt(inner_sig,inner_csig,len,&ctr)) != CRYPT_OK) {
     printf("ctr_encrypt error: %s\n", error_to_string(err));
     return -1;
  }
  len2 = sizeof(sig);
  if ((err = base64_encode(inner_csig, len, (unsigned char*)sig, &len2)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  sig[len2] = 0;
  
  // create the outer open packet
  iat = (char*)outer+2;
  sprintf(iat, "{\"type\":\"open\",\"open\":\"%s\",\"iv\":\"%s\",\"sig\":\"%s\"}",open,iv,sig);
  printf("OUTER: %s\n",iat);
  iatlen = htons(strlen(iat));
  memcpy(outer,&iatlen,2);
  outer_len = 2+strlen(iat);
  memcpy(outer+outer_len,inner_enc,inner_len);
  outer_len += inner_len;
  printf("outer packet len %d: %s\n",outer_len,hex(outer,outer_len,out));  

  // create a udp socket
  if( (sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) ) < 0 )
  {
	  perror("failed to create socket");
	  return -1;
  }
  memset((char *)&sad,0,sizeof(sad));
  sad.sin_family = AF_INET;
  sad.sin_port = htons(0);
  sad.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (sock, (struct sockaddr *)&sad, sizeof(sad)) < 0)
  {
	  perror("bind failed");
	  return -1;
  }

  // send our open
  memset((char *)&seed_ad,0,sizeof(seed_ad));
  seed_ad.sin_family = AF_INET;
  seed_ad.sin_port = htons(seed_port);
  if(inet_aton(seed_ip, &seed_ad.sin_addr)==0)
  {
	  printf("seed ip failed: %s\n",seed_ip);
	  return -1;
  }
  if(sendto(sock, outer, outer_len, 0, (struct sockaddr *)&seed_ad, sizeof(seed_ad))==-1)
  {
	  perror("sendto failed");
	  return -1;
  }

  // receive answer
  len = sizeof(rad);
  if ((x = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&rad, (socklen_t *)&len)) == -1)
  {
	  perror("recvfrom failed");
	  return -1;
  }
  printf("Received packet from %s:%d len %d data: %s\n", inet_ntoa(rad.sin_addr), ntohs(rad.sin_port), x, hex(buf,x,out));

  // process answer
  memcpy(&inlen,buf,2);
  iatlen = ntohs(inlen);
  iat = (char*)buf+2;
  printf("incoming json length %d\n",iatlen);
  if(js0n((unsigned char*)iat,iatlen,index,sizeof(index)))
  {
	  printf("failed to parse json: %.*s\n",iatlen,iat);
	  return -1;
  }
  body = buf+2+iatlen;
  body_len = x-(2+iatlen);
  popen = j0g_str("open",iat,index);
  psig = j0g_str("sig",iat,index);
  piv = j0g_str("iv",iat,index);
  if(!popen || !psig || !piv)
  {
	  printf("incomplete json packet\n");
	  return -1;
  }
  
  // decode and decrypt the open value
  len = sizeof(buf);
  if ((err = base64_decode((unsigned char*)popen, strlen(popen), buf, &len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  len2 = sizeof(seed_eccpub);
  if ((err = rsa_decrypt_key(buf, len, seed_eccpub, &len2, 0, 0, find_hash("sha1"), &x, &self_rsa)) != CRYPT_OK) {
      printf("rsa_decrypt error: %s\n", error_to_string(err));
      return -1;
  }
  printf("seed ecc pub %d %s\n",x,hex(seed_eccpub,len2,out));

  // create the aes cipher key/iv
  len = 32;
  if ((err = hash_memory(find_hash("sha256"),seed_eccpub,65,aeskey,&len)) != CRYPT_OK) {
     printf("Error hashing key: %s\n", error_to_string(err));
     return -1;
  }
  printf("aes key %s ",hex(aeskey,32,out));
  unhex(piv,32,aesiv);
  printf("iv %s\n",hex(aesiv,16,out));

  // create aes cipher now and decrypt the inner
  if ((err = ctr_start(find_cipher("aes"),aesiv,aeskey,32,0,CTR_COUNTER_BIG_ENDIAN,&ctr)) != CRYPT_OK) {
     printf("ctr_start error: %s\n",error_to_string(err));
     return -1;
  }
  if ((err = ctr_decrypt(body,inner,body_len,&ctr)) != CRYPT_OK) {
     printf("ctr_decrypt error: %s\n", error_to_string(err));
     return -1;
  }
  printf("aes decrypt %s\n",hex(inner,body_len,out));
  
  // parse the inner packet
  memcpy(&inlen,inner,2);
  iatlen = ntohs(inlen);
  iat = (char*)inner+2;
  printf("inner json length %d\n",iatlen);
  if(js0n((unsigned char*)iat,iatlen,index,sizeof(index)))
  {
	  printf("failed to parse json: %.*s\n",iatlen,iat);
	  return -1;
  }
  seed_der = inner+2+iatlen;
  seed_der_len = body_len-(2+iatlen);
  seed_line = j0g_str("line",iat,index);
  if(!seed_line || !j0g_str("at",iat,index) || strcmp(j0g_str("to",iat,index),self_hn) != 0)
  {
	  printf("invalid inner json packet: %.*s\n",iatlen,iat);
	  return -1;
  }
  
  // load and verify the inner body key
  if ((err = rsa_import(seed_der, seed_der_len, &seed_rsa)) != CRYPT_OK) {
    printf("Import public error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = hashname(seed_der, seed_der_len, out)) != CRYPT_OK) {
    printf("hashname error: %s\n", error_to_string(err));
    return -1;
  }
  if(strcmp(out,seed_hn) != 0)
  {
	  printf("hashnames don't match %s %s\n",seed_hn,out);
	  return -1;
  }
  
  // verify the signature
  len = sizeof(buf);
  if ((err = base64_decode((unsigned char*)psig, strlen(psig), buf, &len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = ctr_decrypt(buf,csig,len,&ctr)) != CRYPT_OK) {
     printf("ctr_decrypt error: %s\n", error_to_string(err));
     return -1;
  }
	
  len2 = 32;
  if ((err = hash_memory(find_hash("sha256"),body,body_len,sighash,&len2)) != CRYPT_OK) {
     printf("Error hashing key: %s\n", error_to_string(err));
     return -1;
  }
  if ((err = rsa_verify_hash_ex(csig, len, sighash, len2, LTC_PKCS_1_V1_5, find_hash("sha256"), 0, &x, &seed_rsa)) != CRYPT_OK) {
    printf("verify sig error: %s\n", error_to_string(err));
    return -1;
  }
  if(x != 1)
  {
	  printf("signature not valid\n");
	  return -1;
  }

  // do the diffie hellman
  if ((err = ecc_ansi_x963_import(seed_eccpub, 65, &urkey)) != CRYPT_OK) {
	  printf("Error importing ecc key: %s\n", error_to_string(err));
	  return -1;
  }
  printf("ecc pub key %s\n",hex(self_eccpub,65,(char*)out));
  len = sizeof(secret);
  if ((err = ecc_shared_secret(&mykey,&urkey,secret,&len)) != CRYPT_OK) {
    printf("Error doing DH: %s\n", error_to_string(err)); return -1;
  }
  printf("shared secret %s\n",hex(secret,len,out));

  return 0;
}

char *hex(unsigned char *digest, int len, char *out)
{
    int j;
    char *c = out;

    for (j = 0; j < len; j++) {
      sprintf(c,"%02x", digest[j]);
      c += 2;
    }
    *c = '\0';
    return out;
}

unsigned char hexcode(char x)
{
    if (x >= '0' && x <= '9')         /* 0-9 is offset by hex 30 */
      return (x - 0x30);
    else if (x >= 'A' && x <= 'F')    /* A-F offset by hex 37 */
      return(x - 0x37);
    else if (x >= 'a' && x <= 'f')    /* a-f offset by hex 37 */
      return(x - 0x57);
    else {                            /* Otherwise, an illegal hex digit */
		return x;
    }
}

unsigned char *unhex(char *hex, int len, unsigned char *out)
{
	int j;
	unsigned char *c = out;
	for(j=0; (j+1)<len; j+=2)
	{
		*c = ((hexcode(hex[j]) * 16) & 0xF0) + (hexcode(hex[j+1]) & 0xF);
		c++;
	}
	return out;
}

int hashname(unsigned char *key, int klen, char *hn)
{
  unsigned char bin[32];
  unsigned long len=32;
  int err;
  if ((err = hash_memory(find_hash("sha256"),key,klen,bin,&len)) != CRYPT_OK) return err;
  hex(bin,len,hn);
  return CRYPT_OK;
}