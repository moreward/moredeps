#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include <pcre2.h>
int main(void){int err; PCRE2_SIZE off; pcre2_code *re = pcre2_compile((PCRE2_SPTR)"hello", PCRE2_ZERO_TERMINATED, 0, &err, &off, NULL); if(re) pcre2_code_free(re); return 0;}
