#ifndef BENCODER_H
#define BENCODER_H

#include <stdio.h>
#include <stdbool.h>

typedef enum {
    BEN_INT,
    BEN_STR,
    BEN_LIST,
    BEN_DICT
} BencodeType;

typedef struct BencodeNode BencodeNode;

struct BencodeNode {
    BencodeType type;
    long startOffset;
    long endOffset;

    union {
        long intValue;

        struct {
            unsigned char *data;
            size_t length;
        } string;

        struct {
            BencodeNode **items;
            size_t length;
            size_t capacity;
        } list;

        struct {
            char **keys;
            BencodeNode **values;
            size_t length;
            size_t capacity;
        } dict;
    };
};

typedef struct {
    FILE *file;
    bool hasError;
    char errorMsg[256];
    long errorPosition;
} BencodeContext;

void freeBencodeNode(BencodeNode *node);

void reportError(BencodeContext *ctx, const char *format, ...);

BencodeNode *getDictValue(const BencodeNode *dict, const char *key);

#endif
