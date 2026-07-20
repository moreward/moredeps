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
 *
 * What is intentionally NOT wrapped here (use PhysFS directly if you need it):
 *   - Library lifecycle: PHYSFS_init, PHYSFS_deinit.
 *   - Mount / search path management: PHYSFS_mount, PHYSFS_unmount,
 *     PHYSFS_addToSearchPath, PHYSFS_removeFromSearchPath,
 *     PHYSFS_getSearchPath, PHYSFS_getMountPoint, PHYSFS_getRealDir,
 *     PHYSFS_mountIo, PHYSFS_mountMemory, PHYSFS_mountHandle.
 *   - Write directory configuration: PHYSFS_getWriteDir, PHYSFS_setWriteDir,
 *     PHYSFS_setSaneConfig, PHYSFS_getBaseDir, PHYSFS_getUserDir,
 *     PHYSFS_getPrefDir, PHYSFS_getDirSeparator.
 *   - CD-ROM enumeration: PHYSFS_getCdRomDirs, PHYSFS_getCdRomDirsCallback.
 *   - Symlink policy: PHYSFS_permitSymbolicLinks,
 *     PHYSFS_symbolicLinksPermitted.
 *   - Allocator hooks: PHYSFS_setAllocator, PHYSFS_getAllocator.
 *   - String/list helpers: PHYSFS_freeList, PHYSFS_utf8FromUcs2,
 *     PHYSFS_utf8ToUcs2, PHYSFS_ucs2FromUtf8.
 *   - Archive/version/utility: PHYSFS_getSupportedArchiveTypes,
 *     PHYSFS_getLinkedVersion.
 *   - Byte-swap helpers: the PHYSFS_swap* family.
 *   - All deprecated PhysFS functions.
 *
 * What IS wrapped: the file/directory/stat API you would expect from a WASI
 * or Node.js-like filesystem surface, plus whole-file convenience helpers.
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

/* Write data to the PhysFS write directory, replacing the file if it exists.
 * Returns non-zero on success, 0 on failure.
 */
static inline int mfs_write_file(const char *path, const void *data, size_t len)
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
static inline int mfs_append_file(const char *path, const void *data, size_t len)
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

/* ========================================================================= */
/* Streaming file API                                                        */
/* ========================================================================= */

/* Open a file for reading from the PhysFS search path. */
static inline PHYSFS_File *mfs_open_read(const char *path)
{
    return PHYSFS_openRead(path);
}

/* Open a file for writing in the PhysFS write directory. */
static inline PHYSFS_File *mfs_open_write(const char *path)
{
    return PHYSFS_openWrite(path);
}

/* Open a file for appending in the PhysFS write directory. */
static inline PHYSFS_File *mfs_open_append(const char *path)
{
    return PHYSFS_openAppend(path);
}

/* Close an open PhysFS file. Returns non-zero on success. */
static inline int mfs_close(PHYSFS_File *f)
{
    return PHYSFS_close(f) != 0;
}

/* Read up to len bytes from f into buf.
 * Returns the number of bytes read, or -1 on error. */
static inline PHYSFS_sint64 mfs_read(PHYSFS_File *f, void *buf, size_t len)
{
    return PHYSFS_readBytes(f, buf, (PHYSFS_uint64)len);
}

/* Write up to len bytes from buf to f.
 * Returns the number of bytes written, or -1 on error. */
static inline PHYSFS_sint64 mfs_write(PHYSFS_File *f, const void *buf, size_t len)
{
    return PHYSFS_writeBytes(f, buf, (PHYSFS_uint64)len);
}

/* Flush any buffered writes to f. Returns non-zero on success. */
static inline int mfs_flush(PHYSFS_File *f)
{
    return PHYSFS_flush(f) != 0;
}

/* Seek to position pos in f. Returns non-zero on success. */
static inline int mfs_seek(PHYSFS_File *f, PHYSFS_uint64 pos)
{
    return PHYSFS_seek(f, pos) != 0;
}

/* Return the current read/write position in f, or -1 on error. */
static inline PHYSFS_sint64 mfs_tell(PHYSFS_File *f)
{
    return PHYSFS_tell(f);
}

/* Return non-zero if the read position is at the end of f. */
static inline int mfs_eof(PHYSFS_File *f)
{
    return PHYSFS_eof(f) != 0;
}

/* Return the size of f in bytes, or -1 on error. */
static inline PHYSFS_sint64 mfs_file_size(PHYSFS_File *f)
{
    return PHYSFS_fileLength(f);
}

/* ========================================================================= */
/* Path metadata                                                             */
/* ========================================================================= */

/* Fill *out with metadata for path. Returns non-zero on success. */
static inline int mfs_stat(const char *path, PHYSFS_Stat *out)
{
    return PHYSFS_stat(path, out) != 0;
}

/* Return non-zero if path is a directory. */
static inline int mfs_is_dir(const char *path)
{
    PHYSFS_Stat st;
    if (!mfs_stat(path, &st)) return 0;
    return st.filetype == PHYSFS_FILETYPE_DIRECTORY;
}

/* Return non-zero if path is a regular file. */
static inline int mfs_is_file(const char *path)
{
    PHYSFS_Stat st;
    if (!mfs_stat(path, &st)) return 0;
    return st.filetype == PHYSFS_FILETYPE_REGULAR;
}

/* ========================================================================= */
/* Directory operations                                                      */
/* ========================================================================= */

/* Callback for mfs_list. Return 0 to stop, non-zero to continue. */
typedef int (*mfs_list_callback)(void *userdata, const char *name);

/* Internal context used by mfs_list(). */
struct mfs__list_ctx {
    mfs_list_callback cb;
    void *userdata;
};

static PHYSFS_EnumerateCallbackResult mfs__enum_cb(void *data, const char *origdir, const char *fname)
{
    struct mfs__list_ctx *c = (struct mfs__list_ctx *)data;
    (void)origdir;
    return c->cb(c->userdata, fname) ? PHYSFS_ENUM_OK : PHYSFS_ENUM_STOP;
}

/* List entries in a directory. For each entry, cb is called.
 * Returns non-zero on success (even if cb stopped early). */
static inline int mfs_list(const char *path, mfs_list_callback cb, void *userdata)
{
    struct mfs__list_ctx ctx = { cb, userdata };
    return PHYSFS_enumerate(path, mfs__enum_cb, &ctx) != 0;
}

/* Create a directory in the PhysFS write directory.
 * Intermediate directories are created as needed.
 * Returns non-zero on success. */
static inline int mfs_mkdir(const char *path)
{
    return PHYSFS_mkdir(path) != 0;
}

/* Remove a file or empty directory in the PhysFS write directory.
 * Returns non-zero on success. */
static inline int mfs_remove(const char *path)
{
    return PHYSFS_delete(path) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* MFS_H */
