#include <stddef.h>
#include <sqlite3.h>
int main(void) {
    sqlite3 *db;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) return 1;
    char *err = NULL;
    int rc = sqlite3_exec(db,
        "CREATE TABLE t(x);"
        "INSERT INTO t VALUES(42);"
        "SELECT x FROM t WHERE x=42;",
        NULL, NULL, &err);
    if (rc != SQLITE_OK) { sqlite3_free(err); sqlite3_close(db); return 2; }
    sqlite3_close(db);
    return 0;
}
