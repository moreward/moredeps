#include <cjson/cJSON.h>
#include <stdio.h>

int main(void) {
    const char *v = cJSON_Version();
    printf("cJSON %s\n", v);
    return 0;
}
