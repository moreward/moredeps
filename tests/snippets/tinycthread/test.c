#include <tinycthread.h>
#include <stdio.h>
static int worker(void *arg) { (void)arg; return 0; }
int main(void) {
    thrd_t t;
    if (thrd_create(&t, worker, NULL) != thrd_success) return 1;
    thrd_join(t, NULL);
    return 0;
}
