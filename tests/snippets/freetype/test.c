#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>

int main(void) {
    FT_Library library;
    FT_Error err = FT_Init_FreeType(&library);
    if (err) {
        fprintf(stderr, "freetype init failed\n");
        return 1;
    }
    printf("freetype ok\n");
    FT_Done_FreeType(library);
    return 0;
}
