#include <xxhash.h>
#include <stdio.h>

int main(void) {
    printf("xxhash version %d\n", XXH_versionNumber());
    return 0;
}
