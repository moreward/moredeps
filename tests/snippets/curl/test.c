#include <curl/curl.h>
#include <stdio.h>

int main(void) {
    curl_version_info_data *v = curl_version_info(CURLVERSION_NOW);
    printf("curl %s\n", v->version);
    return 0;
}
