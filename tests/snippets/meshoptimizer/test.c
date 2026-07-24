#include <meshoptimizer.h>
#include <stdio.h>

int main(void) {
    unsigned int v = meshopt_vertexFormatSize(MESHOPT_VERTEX_FORMAT_FLOAT3);
    printf("meshopt %u\n", v);
    return 0;
}
