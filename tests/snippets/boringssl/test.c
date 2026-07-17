#include <openssl/aes.h>
int main(void){uint8_t k[16]={0},in[16]={0},out[16]={0};AES_KEY ak;AES_set_encrypt_key(k,128,&ak);AES_encrypt(in,out,&ak);return 0;}
