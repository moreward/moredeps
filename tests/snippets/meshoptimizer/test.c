#include <meshoptimizer.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    unsigned int indices[] = {0, 1, 2};
    float vertices[] = {0,0,0, 1,0,0, 0,1,0};
    unsigned int remap[3];
    size_t n = meshopt_generateVertexRemap(remap, indices, 3, vertices, 3, sizeof(float)*3);
    printf("meshopt %zu\n", (size_t)n);
    (void)n;
    return 0;
}
