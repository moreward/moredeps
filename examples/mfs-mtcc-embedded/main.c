/*
 * examples/mfs-mtcc-embedded/main.c
 *
 * Demonstrates loading a C source file from a PhysFS-mounted archive and
 * compiling it in-memory with mtcc, where the mtcc runtime files
 * (libtcc1.a and builtin headers) are also supplied inside the same archive.
 *
 * At runtime, the archive is mounted and the runtime files are extracted to a
 * temporary host directory so that mtcc's libtcc can find them through the
 * standard filesystem.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libtcc.h>
#include <physfs.h>
#include "mfs.h"

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#else
#include <unistd.h>
#endif

static void host_print(const char *msg)
{
    printf("[jit] %s\n", msg);
}

static void tcc_error_handler(void *opaque, const char *msg)
{
    (void)opaque;
    fprintf(stderr, "mtcc error: %s\n", msg);
}

/* Write a buffer to a host filesystem path. */
static int write_host_file(const char *path, const void *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (len > 0 && fwrite(data, 1, len, f) != len) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

/* Extract a file from PhysFS to the host filesystem. */
static int extract_file(const char *src_path, const char *dst_path)
{
    size_t len;
    void *data = mfs_load(src_path, &len);
    if (!data) {
        fprintf(stderr, "could not load '%s': %s\n", src_path, mfs_last_error());
        return 0;
    }
    int ok = write_host_file(dst_path, data, len);
    free(data);
    return ok;
}

static int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 1;
    if (mkdir(path, 0755) != 0) {
        fprintf(stderr, "mkdir(%s) failed: %s\n", path, strerror(errno));
        return 0;
    }
    return 1;
}

struct extract_ctx {
    const char *src_dir;
    const char *dst_dir;
    int ok;
};

static int extract_dir_cb(void *userdata, const mfs_dir_entry *entry)
{
    struct extract_ctx *ctx = (struct extract_ctx *)userdata;
    if (!ctx->ok) return 0;

    size_t src_len = strlen(ctx->src_dir) + strlen(entry->name) + 2;
    char *src_path = (char *)malloc(src_len);
    size_t dst_len = strlen(ctx->dst_dir) + strlen(entry->name) + 2;
    char *dst_path = (char *)malloc(dst_len);
    if (!src_path || !dst_path) {
        free(src_path); free(dst_path);
        ctx->ok = 0;
        return 0;
    }

    snprintf(src_path, src_len, "%s/%s", ctx->src_dir, entry->name);
    snprintf(dst_path, dst_len, "%s/%s", ctx->dst_dir, entry->name);

    if (entry->is_dir) {
        ensure_dir(dst_path);
        struct extract_ctx child = { src_path, dst_path, 1 };
        mfs_list_ex(src_path, extract_dir_cb, &child);
        ctx->ok = child.ok;
    } else if (entry->is_file) {
        ctx->ok = extract_file(src_path, dst_path);
    }

    free(src_path);
    free(dst_path);
    return ctx->ok;
}

static int extract_dir(const char *src_dir, const char *dst_dir)
{
    if (!ensure_dir(dst_dir)) return 0;
    struct extract_ctx ctx = { src_dir, dst_dir, 1 };
    mfs_list_ex(src_dir, extract_dir_cb, &ctx);
    return ctx.ok;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <zip-or-dir>\n", argv[0]);
        return 1;
    }

    if (!PHYSFS_init(argv[0])) {
        fprintf(stderr, "PHYSFS_init failed: %s\n", mfs_last_error());
        return 1;
    }

    if (!PHYSFS_mount(argv[1], NULL, 1)) {
        fprintf(stderr, "PHYSFS_mount(%s) failed: %s\n", argv[1], mfs_last_error());
        PHYSFS_deinit();
        return 1;
    }

    /* Create a temporary host directory for the mtcc runtime files. */
    char tmpdir[] = "/tmp/mfs_mtcc_embedded_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "mkdtemp failed: %s\n", strerror(errno));
        PHYSFS_deinit();
        return 1;
    }

    int ok = 1;

    /* Extract libtcc1.a and the builtin headers from the archive. */
    char libtcc1_path[512];
    snprintf(libtcc1_path, sizeof(libtcc1_path), "%s/libtcc1.a", tmpdir);
    if (!extract_file("runtime/libtcc1.a", libtcc1_path)) { ok = 0; }

    char include_dst[512];
    snprintf(include_dst, sizeof(include_dst), "%s/include", tmpdir);
    if (!extract_dir("runtime/include", include_dst)) { ok = 0; }

    if (!ok) {
        fprintf(stderr, "failed to extract mtcc runtime from archive\n");
        PHYSFS_deinit();
        return 1;
    }

    /* Set up mtcc. */
    TCCState *s = tcc_new();
    if (!s) {
        fprintf(stderr, "tcc_new failed\n");
        PHYSFS_deinit();
        return 1;
    }

    tcc_set_error_func(s, NULL, tcc_error_handler);
    tcc_set_lib_path(s, tmpdir); /* must be set before tcc_set_output_type */
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    /* Load and compile the JIT source from the archive. */
    size_t len;
    char *src = mfs_load_text("jit/main.c", &len);
    if (!src) {
        fprintf(stderr, "could not load jit/main.c: %s\n", mfs_last_error());
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
