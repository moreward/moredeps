/*
 * mfs.h — minimal, zero-overhead file I/O helpers backed by PhysicsFS.
 *
 * This header is intentionally small and uses static inline functions so the
 * compiler can optimize the wrapper away. It does NOT pull in any runtime
 * indirection (no vtables, no function pointers) beyond the PhysFS calls
 * themselves.
 *
 * The caller is responsible for initializing PhysFS with PHYSFS_init() and
 * mounting the desired archives/directories before using these helpers.
 *
 * Thread safety: PhysFS is not thread-safe by default. If you use mfs from
 * multiple threads, either serialize access or use per-thread PhysFS states.
 */

#ifndef MFS_H
#define MFS_H

#include <stddef.h>
#include <stdlib.h>
#include <physfs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read the whole contents of an open PhysFS file into a caller-owned buffer.
 * On success returns a non-NULL pointer and writes the byte count to *out_size.
 * On failure returns NULL and leaves *out_size unchanged.
 * The returned buffer is NOT null-terminated. If the file is empty, a valid
 * one-byte pointer is returned and *out_size is set to 0.
 */
static inline void *mfs__read_all(PHYSFS_File *f, size_t *out_size)
{
    PHYSFS_sint64 len64 = PHYSFS_fileLength(f);
    if (len64 < 0) return NULL;

    size_t len = (size_t)len64;
    /* Allocate at least one byte so an empty file is distinguishable from
     * an out-of-memory error. */
    void *buf = malloc(len ? len : 1);
    if (!buf) return NULL;

    if (len == 0) {
        *out_size = 0;
        return buf;
    }

    PHYSFS_sint64 total = 0;
    while (total < (PHYSFS_sint64)len) {
        PHYSFS_sint64 n = PHYSFS_readBytes(f, (char *)buf + total,
                                          (PHYSFS_uint64)(len - total));
        if (n <= 0) {
            free(buf);
            return NULL;
        }
        total += n;
    }

    *out_size = (size_t)total;
    return buf;
}

/* Load a text file into a null-terminated malloc'd buffer.
 * Returns NULL on error. out_size may be NULL.
 * Caller must free() the returned pointer.
 */
static inline char *mfs_load_text(const char *path, size_t *out_size)
{
    PHYSFS_File *f = PHYSFS_openRead(path);
    if (!f) return NULL;

    size_t size = 0;
    char *buf = (char *)mfs__read_all(f, &size);
    PHYSFS_close(f);
    if (!buf) return NULL;

    buf[size] = '\0';
    if (out_size) *out_size = size;
    return buf;
}

/* Load a binary file into a malloc'd buffer.
 * Returns NULL on error. out_size is mandatory and receives the byte count.
 * Caller must free() the returned pointer. The buffer is NOT null-terminated.
 */
static inline void *mfs_load(const char *path, size_t *out_size)
{
    if (!out_size) return NULL;

    PHYSFS_File *f = PHYSFS_openRead(path);
    if (!f) return NULL;

    void *buf = mfs__read_all(f, out_size);
    PHYSFS_close(f);
    return buf;
}

/* Write data to the PhysFS write directory.
 * Returns non-zero on success, 0 on failure.
 */
static inline int mfs_write(const char *path, const void *data, size_t len)
{
    PHYSFS_File *f = PHYSFS_openWrite(path);
    if (!f) return 0;
    PHYSFS_sint64 wrote = PHYSFS_writeBytes(f, data, (PHYSFS_uint64)len);
    PHYSFS_close(f);
    return wrote == (PHYSFS_sint64)len;
}

/* Append data to an existing file in the PhysFS write directory.
 * Returns non-zero on success, 0 on failure.
 */
static inline int mfs_append(const char *path, const void *data, size_t len)
{
    PHYSFS_File *f = PHYSFS_openAppend(path);
    if (!f) return 0;
    PHYSFS_sint64 wrote = PHYSFS_writeBytes(f, data, (PHYSFS_uint64)len);
    PHYSFS_close(f);
    return wrote == (PHYSFS_sint64)len;
}

/* Check if a path exists in the mounted PhysFS virtual filesystem. */
static inline int mfs_exists(const char *path)
{
    return PHYSFS_exists(path) != 0;
}

/* Return a human-readable error message for the last PhysFS failure. */
static inline const char *mfs_last_error(void)
{
    return PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
}

#ifdef __cplusplus
}
#endif

#endif /* MFS_H */
