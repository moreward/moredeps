#include <lz4.h>
#include <stdio.h>

int main(void) {
    printf("lz4 version %d\n", LZ4_versionNumber());
    return 0;
}
