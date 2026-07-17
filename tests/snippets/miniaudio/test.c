#include <miniaudio/miniaudio.h>
#include <stdio.h>

int main(void) {
    const char *v = ma_version_string();
    printf("miniaudio %s\n", v);
    return 0;
}
