#define GHOSTTY_STATIC
#include <stdio.h>
#include "ghostty.h"

static const char* mode_name(ghostty_build_mode_e m) {
    switch (m) {
        case GHOSTTY_BUILD_MODE_DEBUG: return "debug";
        case GHOSTTY_BUILD_MODE_RELEASE_SAFE: return "release_safe";
        case GHOSTTY_BUILD_MODE_RELEASE_FAST: return "release_fast";
        case GHOSTTY_BUILD_MODE_RELEASE_SMALL: return "release_small";
    }
    return "unknown";
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    int rc = ghostty_init(0, NULL);
    if (rc != GHOSTTY_SUCCESS) {
        fprintf(stderr, "ghostty_init failed: %d\n", rc);
        return 1;
    }

    ghostty_info_s info = ghostty_info();
    printf("ghostty: version=%.*s mode=%s\n",
           (int)info.version_len,
           info.version ? info.version : "(null)",
           mode_name(info.build_mode));

    ghostty_config_t cfg = ghostty_config_new();
    if (!cfg) {
        fprintf(stderr, "ghostty_config_new failed\n");
        return 1;
    }
    ghostty_config_finalize(cfg);
    ghostty_config_free(cfg);
    printf("config create/finalize/free OK\n");
    return 0;
}
