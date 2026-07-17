#include <sokol_gp/sokol_gfx.h>
#include <sokol_gp/sokol_gp.h>
#include <stdio.h>

int main(void) {
    printf("sgp valid: %d\n", (int)sgp_is_valid());
    return 0;
}
