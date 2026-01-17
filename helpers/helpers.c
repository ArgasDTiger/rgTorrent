#include "helpers.h"
#include <stdbool.h>
#include <stdio.h>

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