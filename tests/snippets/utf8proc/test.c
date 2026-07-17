#include <utf8proc.h>
#include <stdio.h>

int main(void) {
    const char *v = utf8proc_version();
    printf("utf8proc %s\n", v);
    return 0;
}
