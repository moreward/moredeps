#include <nanovg.h>
int main(void){NVGparams p={0};NVGcontext*c=nvgCreateInternal(&p);if(c)nvgDeleteInternal(c);return 0;}
