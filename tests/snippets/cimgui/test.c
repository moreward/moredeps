#include <cimgui.h>
int main(void){
    ImGuiContext* ctx = igCreateContext(NULL);
    igDestroyContext(ctx);
    return 0;
}
