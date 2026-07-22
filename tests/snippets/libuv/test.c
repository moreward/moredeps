#include <uv.h>
int main(void) {
    uv_loop_t *loop = uv_default_loop();
    if (!loop) return 1;
    uv_run(loop, UV_RUN_NOWAIT);
    uv_loop_close(loop);
    return 0;
}
