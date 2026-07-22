#include <toml.h>
#include <stdio.h>
int main(void) {
    char errbuf[256];
    toml_table_t *root = toml_parse("key = 42\n", errbuf, sizeof(errbuf));
    if (!root) { fprintf(stderr, "%s\n", errbuf); return 1; }
    toml_free(root);
    return 0;
}
