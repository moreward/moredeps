#include <utf8proc.h>
#include <string.h>
int main(void) {
    utf8proc_int32_t buf[64];
    utf8proc_ssize_t n = utf8proc_decompose(
        (const utf8proc_uint8_t*)"\xc3\xa9", 2, buf, sizeof(buf)/sizeof(buf[0]),
        UTF8PROC_DECOMPOSE | UTF8PROC_STRIPMARK);
    if (n < 0) return 1;
    /* "\xc3\xa9" (é) decomposes to "e" + combining accent;
     * with STRIPMARK it becomes just "e" */
    return (n == 1 && buf[0] == 'e') ? 0 : 2;
}
