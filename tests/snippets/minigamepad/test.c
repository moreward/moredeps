#include <minigamepad.h>
#include <stdio.h>
int main(void){const char *n=mg_axis_get_name(0);printf("mg %s\n",n?n:"null");return 0;}
