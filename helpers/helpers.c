#include "helpers.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool isDigit(const int ch) {
    return ch >= 48 && ch <= 57;
}

int fpeek(FILE *const fp) {
    const int c = fgetc(fp);
    if (c != EOF) {
        ungetc(c, fp);
    }
    return c;
}

void rand_str(unsigned char *dest, size_t length) {
    const static char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        const size_t index = (size_t) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}