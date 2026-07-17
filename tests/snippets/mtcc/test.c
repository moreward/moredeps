#include <libtcc.h>
int main(void){TCCState *s=tcc_new();if(s)tcc_delete(s);return 0;}
