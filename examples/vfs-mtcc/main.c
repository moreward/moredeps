/*
 * examples/vfs-mtcc/main.c
 *
 * Demonstrates loading a C source file from a PhysicsFS-mounted archive,
 * compiling it in-memory with mtcc, and running it with only a small,
 * explicitly-whitelisted set of host symbols exposed.
 *
 * SECURITY: this is only safe for the *source-loading* step. The compiled
 * code still runs natively in the host process. To sandbox untrusted code you
 * must either (a) whitelist only harmless symbols, or (b) run the compiled
 * function in a separate process.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libtcc.h>
#include <physfs.h>
#include "md_vfs.h"

/* A deliberately minimal host function we expose to the JIT code.
 * Nothing else from libc or the host is reachable.
 */
static void host_print(const char *msg)
{
    printf("[jit] %s\n", msg);
}

static void tcc_error_handler(void *opaque, const char *msg)
{
    (void)opaque;
    fprintf(stderr, "mtcc error: %s\n", msg);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <zip-or-dir>\n", argv[0]);
        return 1;
    }

    if (!PHYSFS_init(argv[0])) {
        fprintf(stderr, "PHYSFS_init failed: %s\n", md_vfs_last_error());
        return 1;
    }

    if (!PHYSFS_mount(argv[1], NULL, 1)) {
        fprintf(stderr, "PHYSFS_mount(%s) failed: %s\n", argv[1], md_vfs_last_error());
        PHYSFS_deinit();
        return 1;
    }

    TCCState *s = tcc_new();
    if (!s) {
        fprintf(stderr, "tcc_new failed\n");
        PHYSFS_deinit();
        return 1;
    }

    tcc_set_error_func(s, NULL, tcc_error_handler);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    /* Do NOT add any include paths or library paths. The source must be
     * self-contained and cannot #include system headers.
     */

    size_t len;
    char *src = md_vfs_load_text("main.c", &len);
    if (!src) {
        fprintf(stderr, "could not load main.c: %s\n", md_vfs_last_error());
        tcc_delete(s);
        PHYSFS_deinit();
        return 1;
    }

    if (tcc_compile_string(s, src) != 0) {
        fprintf(stderr, "tcc_compile_string failed\n");
        free(src);
        tcc_delete(s);
        PHYSFS_deinit();
        return 1;
    }
    free(src);

    /* Expose only the symbols we want the JIT code to be able to call.
     * No libc, no file I/O, no network.
     */
    tcc_add_symbol(s, "host_print", (void *)host_print);

    if (tcc_relocate(s) < 0) {
        fprintf(stderr, "tcc_relocate failed\n");
        tcc_delete(s);
        PHYSFS_deinit();
        return 1;
    }

    void (*run)(void) = (void (*)(void))tcc_get_symbol(s, "run");
    if (!run) {
        fprintf(stderr, "symbol 'run' not found\n");
        tcc_delete(s);
        PHYSFS_deinit();
        return 1;
    }

    run();

    tcc_delete(s);
    PHYSFS_deinit();
    return 0;
}
