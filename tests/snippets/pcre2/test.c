#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include <pcre2.h>
int main(void) {
    int err; PCRE2_SIZE off;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)"h.llo", PCRE2_ZERO_TERMINATED, 0, &err, &off, NULL);
    if (!re) return 1;
    pcre2_match_data *md = pcre2_match_data_create(1, NULL);
    if (!md) { pcre2_code_free(re); return 1; }
    int rc = pcre2_match(re, (PCRE2_SPTR)"hello", 5, 0, 0, md, NULL);
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return rc >= 0 ? 0 : 2;
}
