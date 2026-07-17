#include <sqlite3.h>
#include <stdio.h>

int main(void) {
    const char *v = sqlite3_libversion();
    printf("sqlite %s\n", v);
    return 0;
}
