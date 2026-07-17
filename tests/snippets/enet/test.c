#include <enet.h>
#include <stdio.h>

int main(void) {
    if (enet_initialize() != 0) {
        fprintf(stderr, "enet init failed\n");
        return 1;
    }
    printf("enet ok\n");
    enet_deinitialize();
    return 0;
}
