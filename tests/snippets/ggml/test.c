#include <ggml.h>
#include <stdlib.h>
int main(void) {
    struct ggml_init_params params = { .mem_size = 64*1024, .mem_buffer = NULL, .no_alloc = false };
    struct ggml_context *ctx = ggml_init(params);
    if (!ctx) return 1;
    struct ggml_tensor *t = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    if (!t) { ggml_free(ctx); return 2; }
    ggml_set_name(t, "test");
    ggml_free(ctx);
    return 0;
}
