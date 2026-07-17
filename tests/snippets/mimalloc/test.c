#include <mimalloc.h>
#include <stdio.h>

int main(void) {
    int v = mi_version();
    printf("mimalloc version %d\n", v);
    return 0;
}
