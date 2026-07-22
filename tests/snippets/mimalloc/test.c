#include <mimalloc.h>
#include <stdlib.h>
int main(void) {
    void *p = mi_malloc(64);
    if (!p) return 1;
    for (int i = 0; i < 64; i++) ((char*)p)[i] = (char)i;
    void *p2 = mi_realloc(p, 128);
    if (!p2) { mi_free(p); return 1; }
    for (int i = 0; i < 64; i++) if (((char*)p2)[i] != (char)i) return 2;
    mi_free(p2);
    return 0;
}
