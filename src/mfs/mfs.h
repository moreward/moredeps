/*
 * md_vfs.h — minimal, zero-overhead file I/O helpers backed by PhysicsFS.
 *
 * This header is intentionally small and uses static inline functions so the
 * compiler can optimize the wrapper away. It does NOT pull in any runtime
 * indirection (no vtables, no function pointers) beyond the PhysFS calls
 * themselves.
 *
 * The caller is responsible for initializing PhysFS with PHYSFS_init() and
 * mounting the desired archives/directories before using these helpers.
 *
 * Thread safety: PhysFS is not thread-safe by default. If you use md_vfs from
 * multiple threads, either serialize access or use per-thread PhysFS states.
 */

#ifndef MD_VFS_H
#define MD_VFS_H

#include <stddef.h>
#include <stdlib.h>
#include <physfs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Load a text file into a null-terminated malloc'd buffer.
 * Returns NULL on error. out_size may be NULL.
 * Caller must free() the returned pointer.
 */
static inline char *md_vfs_load_text(const char *path, size_t *out_size)
{
    PHYSFS_File *f = PHYSFS_openRead(path);
    if (!f) return NULL;

    PHYSFS_sint64 len = PHYSFS_fileLength(f);
    if (len < 0) { PHYSFS_close(f); return NULL; }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { PHYSFS_close(f); return NULL; }

    PHYSFS_sint64 total = 0;
    while (total < len) {
        PHYSFS_sint64 n = PHYSFS_readBytes(f, buf + total,
                                          (PHYSFS_uint64)(len - total));
        if (n <= 0) break;
        total += n;
    }
    PHYSFS_close(f);

    if (total < len) { free(buf); return NULL; }
    buf[total] = '\0';
    if (out_size) *out_size = (size_t)total;
    return buf;
}

/* Load a binary file into a malloc'd buffer.
 * Returns NULL on error. out_size is mandatory and receives the byte count.
 * Caller must free() the returned pointer. The buffer is NOT null-terminated.
 */
static inline void *md_vfs_load(const char *path, size_t *out_size)
{
    PHYSFS_File *f = PHYSFS_openRead(path);
    if (!f) return NULL;

    PHYSFS_sint64 len = PHYSFS_fileLength(f);
    if (len < 0) { PHYSFS_close(f); return NULL; }

    void *buf = malloc((size_t)len);
    if (!buf) { PHYSFS_close(f); return NULL; }

    PHYSFS_sint64 total = 0;
    while (total < len) {
        PHYSFS_sint64 n = PHYSFS_readBytes(f, (char *)buf + total,
                                          (PHYSFS_uint64)(len - total));
        if (n <= 0) break;
        total += n;
    }
    PHYSFS_close(f);

    if (total < len) { free(buf); return NULL; }
    *out_size = (size_t)total;
    return buf;
}

/* Write data to the PhysFS write directory.
 * Returns non-zero on success, 0 on failure.
 */
static inline int md_vfs_write(const char *path, const void *data, size_t len)
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
static inline int md_vfs_append(const char *path, const void *data, size_t len)
{
    PHYSFS_File *f = PHYSFS_openAppend(path);
    if (!f) return 0;
    PHYSFS_sint64 wrote = PHYSFS_writeBytes(f, data, (PHYSFS_uint64)len);
    PHYSFS_close(f);
    return wrote == (PHYSFS_sint64)len;
}

/* Check if a path exists in the mounted PhysFS virtual filesystem. */
static inline int md_vfs_exists(const char *path)
{
    return PHYSFS_exists(path) != 0;
}

/* Return a human-readable error message for the last PhysFS failure. */
static inline const char *md_vfs_last_error(void)
{
    return PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
}

#ifdef __cplusplus
}
#endif

#endif /* MD_VFS_H */
