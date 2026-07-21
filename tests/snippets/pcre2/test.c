#include <stddef.h>
#include <pcre2.h>
int main(void){int err; size_t off; pcre2_code *re = pcre2_compile((PCRE2_SPTR)"hello", PCRE2_ZERO_TERMINATED, 0, &err, &off, NULL); if(re) pcre2_code_free(re); return 0;}
