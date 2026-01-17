#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "bencoder.h"

void freeBencodeNode(BencodeNode *node) {
    if (node == NULL) return;

    switch (node->type) {
        case BEN_INT:
            break;
        case BEN_STR:
            if (node->string.data != NULL) {
                free(node->string.data);
            }
            break;
        case BEN_LIST:
            for (size_t i = 0; i < node->list.length; i++) {
                freeBencodeNode(node->list.items[i]);
            }
            free(node->list.items);
            break;
        case BEN_DICT:
            for (size_t i = 0; i < node->dict.length; i++) {
                free(node->dict.keys[i]);
                freeBencodeNode(node->dict.values[i]);
            }
            free(node->dict.keys);
            free(node->dict.values);
    }

    free(node);
}

void reportError(BencodeContext *ctx, const char *format, ...) {
    ctx->hasError = true;
    ctx->errorPosition = ftell(ctx->file);

    va_list args;
    va_start(args, format);
    vsnprintf(ctx->errorMsg, sizeof(ctx->errorMsg), format, args);
    va_end(args);
}

BencodeNode *getDictValue(const BencodeNode *dict, const char *key) {
    if (dict == NULL || dict->type != BEN_DICT) {
        // TODO: log an error
        return NULL;
    };

    for (size_t i = 0; i < dict->dict.length; i++) {
        if (strcmp(dict->dict.keys[i], key) == 0) {
            return dict->dict.values[i];
        }
    }
    return NULL;
}
