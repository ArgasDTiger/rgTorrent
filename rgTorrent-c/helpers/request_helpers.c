#include "request_helpers.h"

void url_encode(const unsigned char *src, const int len, char *dst) {
    for (int i = 0; i < len; i++) {
        if ((src[i] >= 'a' && src[i] <= 'z') ||
            (src[i] >= 'A' && src[i] <= 'Z') ||
            (src[i] >= '0' && src[i] <= '9') ||
            src[i] == '.' || src[i] == '-' || src[i] == '_' || src[i] == '~') {
            *dst++ = (char)src[i];
            } else {
            const char hex[] = "0123456789ABCDEF";
            *dst++ = '%';
                *dst++ = hex[src[i] >> 4];
                *dst++ = hex[src[i] & 15];
            }
    }
    *dst = '\0';
}