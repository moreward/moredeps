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
 * This header wraps the vast majority of the PhysFS API surface.  The only
 * things intentionally left out are:
 *   - Library lifecycle: PHYSFS_init, PHYSFS_deinit, PHYSFS_isInit.
 *   - Allocator hooks: PHYSFS_setAllocator, PHYSFS_getAllocator.
 *   - String/list helpers: PHYSFS_freeList, PHYSFS_utf8FromUcs2,
 *     PHYSFS_utf8ToUcs2, PHYSFS_ucs2FromUtf8.
 *   - Archive/version/utility: PHYSFS_getSupportedArchiveTypes,
 *     PHYSFS_getLinkedVersion.
 *   - Byte-swap helpers: the PHYSFS_swap* family.
 *   - All deprecated PhysFS functions.
 *   - PHYSFS_mountIo, PHYSFS_mountMemory, PHYSFS_mountHandle (advanced).
 */

#ifndef MFS_H
#define MFS_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <physfs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read the whole contents of an open PhysFS file into a caller-owned buffer.
 * On success returns a non-NULL pointer and writes the byte count to *out_size.
 * On failure returns NULL and leaves *out_size unchanged.
 * The returned buffer is NOT null-terminated, but one extra byte is allocated
 * so text callers can safely append '\0' at index *out_size. If the file is
 * empty, a valid pointer is returned and *out_size is set to 0.
 */
static inline void *mfs__read_all(PHYSFS_File *f, size_t *out_size)
{
    PHYSFS_sint64 len64 = PHYSFS_fileLength(f);
    if (len64 < 0) return NULL;

    size_t len = (size_t)len64;
    /* Allocate one extra byte: (a) callers can null-terminate when
     * treating the buffer as text, and (b) an empty file returns a
     * non-NULL pointer so it's distinguishable from out-of-memory. */
    void *buf = malloc(len + 1);
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
 * Returns non-zero on success, 0 on failure. */
static inline int mfs_write_file(const char *path, const void *data, size_t len)
{
    PHYSFS_File *f = PHYSFS_openWrite(path);
    if (!f) return 0;
    PHYSFS_sint64 wrote = PHYSFS_writeBytes(f, data, (PHYSFS_uint64)len);
    int closed = PHYSFS_close(f);
    return (wrote == (PHYSFS_sint64)len) && closed;
}

/* Append data to an existing file in the PhysFS write directory.
 * Returns non-zero on success, 0 on failure. */
static inline int mfs_append_file(const char *path, const void *data, size_t len)
{
    PHYSFS_File *f = PHYSFS_openAppend(path);
    if (!f) return 0;
    PHYSFS_sint64 wrote = PHYSFS_writeBytes(f, data, (PHYSFS_uint64)len);
    int closed = PHYSFS_close(f);
    return (wrote == (PHYSFS_sint64)len) && closed;
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

/* Return non-zero if path is a symbolic link. */
static inline int mfs_is_symlink(const char *path)
{
    PHYSFS_Stat st;
    if (!mfs_stat(path, &st)) return 0;
    return st.filetype == PHYSFS_FILETYPE_SYMLINK;
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

/* Rich directory listing: metadata per entry. */

typedef struct {
    const char *name;          /* entry name (only valid during the callback) */
    int is_dir;                /* non-zero if directory */
    int is_file;               /* non-zero if regular file */
    int is_symlink;            /* non-zero if symlink */
    PHYSFS_sint64 size;        /* file size in bytes, -1 if unknown or not a file */
    int readonly;              /* non-zero if PhysFS considers it read-only */
} mfs_dir_entry;

/* Callback for mfs_list_ex. Return 0 to stop, non-zero to continue. */
typedef int (*mfs_list_ex_callback)(void *userdata, const mfs_dir_entry *entry);

struct mfs__list_ex_ctx {
    mfs_list_ex_callback cb;
    void *userdata;
};

static PHYSFS_EnumerateCallbackResult mfs__enum_ex_cb(void *data, const char *origdir, const char *fname)
{
    struct mfs__list_ex_ctx *c = (struct mfs__list_ex_ctx *)data;
    size_t dir_len = strlen(origdir);
    size_t full_len = dir_len + strlen(fname) + 2;
    char *full = (char *)malloc(full_len);
    if (!full) return PHYSFS_ENUM_ERROR;
    if (dir_len == 0) {
        snprintf(full, full_len, "%s", fname);
    } else {
        snprintf(full, full_len, "%s/%s", origdir, fname);
    }

    PHYSFS_Stat st;
    mfs_dir_entry entry = { fname, 0, 0, 0, -1, 0 };
    if (PHYSFS_stat(full, &st)) {
        entry.is_dir = (st.filetype == PHYSFS_FILETYPE_DIRECTORY);
        entry.is_file = (st.filetype == PHYSFS_FILETYPE_REGULAR);
        entry.is_symlink = (st.filetype == PHYSFS_FILETYPE_SYMLINK);
        entry.size = st.filesize;
        entry.readonly = st.readonly;
    }
    free(full);

    int cont = c->cb(c->userdata, &entry);
    return cont ? PHYSFS_ENUM_OK : PHYSFS_ENUM_STOP;
}

/* List entries in a directory with metadata.
 * For each entry, cb is called with an mfs_dir_entry. The name pointer is only
 * valid during the callback; copy it if you need to keep it.
 * Returns non-zero on success (even if cb stopped early). */
static inline int mfs_list_ex(const char *path, mfs_list_ex_callback cb, void *userdata)
{
    struct mfs__list_ex_ctx ctx = { cb, userdata };
    return PHYSFS_enumerate(path, mfs__enum_ex_cb, &ctx) != 0;
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

/* ========================================================================= */
/* Mount / search path management                                           */
/* ========================================================================= */

/* Add an archive or directory to the search path.
 * mountPoint may be NULL (equivalent to "/").
 * Returns non-zero on success. */
static inline int mfs_mount(const char *dir, const char *mountPoint, int append)
{
    return PHYSFS_mount(dir, mountPoint, append) != 0;
}

/* Remove a directory or archive from the search path.
 * Returns non-zero on success. */
static inline int mfs_unmount(const char *dir)
{
    return PHYSFS_unmount(dir) != 0;
}

/* Get the real filesystem path where a file resides.
 * Returns a READ-ONLY string, or NULL if not found. */
static inline const char *mfs_get_real_dir(const char *path)
{
    return PHYSFS_getRealDir(path);
}

/* Get the mount point for a previously-added archive/dir.
 * Returns a READ-ONLY string, or NULL on failure. */
static inline const char *mfs_get_mount_point(const char *dir)
{
    return PHYSFS_getMountPoint(dir);
}

/* Collect search path entries via callback.
 * Return 0 from cb to stop, non-zero to continue.
 * cb is called once per entry with the userdata and the path string. */
typedef int (*mfs_search_path_callback)(void *userdata, const char *entry);

struct mfs__sp_ctx {
    mfs_search_path_callback cb;
    void *userdata;
    int cont;
};

static void mfs__sp_cb(void *data, const char *str)
{
    struct mfs__sp_ctx *ctx = (struct mfs__sp_ctx *)data;
    if (ctx->cont) {
        ctx->cont = ctx->cb(ctx->userdata, str);
    }
}

/* Iterate the current search path. Calls cb for each entry.
 * Returns non-zero if all entries were visited (cb never returned 0). */
static inline int mfs_get_search_path(mfs_search_path_callback cb, void *userdata)
{
    struct mfs__sp_ctx ctx = { cb, userdata, 1 };
    PHYSFS_getSearchPathCallback(mfs__sp_cb, &ctx);
    return ctx.cont;
}

/* ========================================================================= */
/* Write / base / user / pref directories                                   */
/* ========================================================================= */

/* Get the application base directory (where the app was run from).
 * Returns a READ-ONLY string, never NULL after PHYSFS_init(). */
static inline const char *mfs_get_base_dir(void)
{
    return PHYSFS_getBaseDir();
}

/* Get the current write directory, or NULL if none set. */
static inline const char *mfs_get_write_dir(void)
{
    return PHYSFS_getWriteDir();
}

/* Set the write directory. Returns non-zero on success. */
static inline int mfs_set_write_dir(const char *path)
{
    return PHYSFS_setWriteDir(path) != 0;
}

/* Get the user's home directory (deprecated in PhysFS, but provided for LFS compat).
 * Returns a READ-ONLY string. */
static inline const char *mfs_get_user_dir(void)
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    return PHYSFS_getUserDir();
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
}

/* Get the platform-specific preferences directory.
 * org and app must be valid non-NULL strings.
 * Returns a READ-ONLY string. */
static inline const char *mfs_get_pref_dir(const char *org, const char *app)
{
    return PHYSFS_getPrefDir(org, app);
}

/* Get the platform-dependent directory separator. */
static inline const char *mfs_get_dir_separator(void)
{
    return PHYSFS_getDirSeparator();
}

/* ========================================================================= */
/* Symlink policy                                                            */
/* ========================================================================= */

/* Enable or disable following of symbolic links. */
static inline void mfs_permit_symlinks(int allow)
{
    PHYSFS_permitSymbolicLinks(allow);
}

/* Return non-zero if symlinks are currently permitted. */
static inline int mfs_symlinks_permitted(void)
{
    return PHYSFS_symbolicLinksPermitted();
}

/* ========================================================================= */
/* Touch: update file modification time                                      */
/* ========================================================================= */

/* Touch a file: open for append and close to update modification time.
 * If the file does not exist, it is created (empty) in the write directory.
 * Returns non-zero on success, 0 on failure. */
static inline int mfs_touch(const char *path)
{
    if (mfs_exists(path)) {
        PHYSFS_File *f = PHYSFS_openAppend(path);
        if (!f) return 0;
        int ok = PHYSFS_close(f);
        return ok != 0;
    }
    /* Create a new empty file. */
    PHYSFS_File *f = PHYSFS_openWrite(path);
    if (!f) return 0;
    int ok = PHYSFS_close(f);
    return ok != 0;
}

/* ========================================================================= */
/* setRoot: offset the root of a mounted archive                             */
/* ========================================================================= */

/* Set the root of an already-mounted archive to a subdirectory.
 * Useful on Android for APK assets: mfs_set_root(apk_path, "/assets")
 * Returns non-zero on success. */
static inline int mfs_set_root(const char *archive, const char *subdir)
{
    return PHYSFS_setRoot(archive, subdir) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* MFS_H */
