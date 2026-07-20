/* examples/mfs-mtcc-embedded/jit/main.c
 *
 * This code is compiled in-memory by the host. It intentionally includes a
 * standard header to demonstrate that the mtcc builtin headers are being
 * loaded from the PhysFS archive.
 */

#include <stddef.h>

extern void host_print(const char *msg);

void run(void)
{
    size_t n = 42;
    host_print("hello from PhysFS + mtcc (runtime files from archive)");
    if (n == 42) {
        host_print("stddef.h was included successfully");
    }
}
