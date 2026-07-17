#include <stddef.h>
#include <fontstash.h>
int main(void){FONSparams p={0};FONScontext *c=fonsCreateInternal(&p);if(c)fonsDeleteInternal(c);return 0;}
