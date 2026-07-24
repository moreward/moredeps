#include <png.h>
#include <stdio.h>

int main(void) {
    png_structp p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    printf("libpng %s\n", p ? "OK" : "FAIL");
    if (p) png_destroy_read_struct(&p, NULL, NULL);
    return 0;
}
