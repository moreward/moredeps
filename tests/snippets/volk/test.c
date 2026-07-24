#include <volk.h>
#include <stdio.h>

int main(void) {
    VkResult r = volkInitialize();
    printf("volk %d\n", r);
    return 0;
}
