#include <zstd.h>
#include <stdio.h>
int main(void){unsigned v=ZSTD_versionNumber();printf("zstd %u\n",v);return 0;}
