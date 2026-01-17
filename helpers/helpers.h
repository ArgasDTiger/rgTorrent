#ifndef HELPERS_H
#define HELPERS_H
#include <stdbool.h>
#include <stdio.h>

bool isDigit(int ch);

int fpeek(FILE *fp);

void rand_str(unsigned char *dest, size_t length);
#endif
