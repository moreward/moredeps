#include <stddef.h>
#include <stdio.h>
#include <jpeglib.h>

int main(void) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    printf("libjpeg-turbo OK\n");
    jpeg_destroy_decompress(&cinfo);
    return 0;
}
