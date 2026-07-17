#include <harfbuzz/hb.h>
#include <stdio.h>

int main(void) {
    const char *v = hb_version_string();
    printf("harfbuzz %s\n", v);
    return 0;
}
