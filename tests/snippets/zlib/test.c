#include <zlib.h>
#include <stdio.h>

int main(void) {
    const char *v = zlibVersion();
    printf("zlib %s\n", v);
    return 0;
}
