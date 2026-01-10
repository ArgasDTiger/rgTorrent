#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define DIGITS_IN_LONG 18
#define DEFAULT_COLLECTION_ITEMS 3

typedef enum {
    BEN_INT,
    BEN_STR,
    BEN_LIST,
    BEN_DICT
} BencodeType;

typedef struct BencodeNode BencodeNode;

struct BencodeNode {
    BencodeType type;

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

BencodeNode *parseList(BencodeContext *ctx);

BencodeNode *parseDict(BencodeContext *ctx);

BencodeNode *parseString(BencodeContext *ctx);

BencodeNode *parseInt(BencodeContext *ctx);

BencodeNode *parseCollectionValue(BencodeContext *ctx);

void freeBencodeNode(BencodeNode *node);

void reportError(BencodeContext *ctx, const char *format, ...);

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

// TODO: handle edge cases https://en.wikipedia.org/wiki/Bencode

void extractNumber(BencodeContext *ctx, long *resultNumber) {
    char numbersBuffer[DIGITS_IN_LONG + 2];
    int position = 0;

    int ch = fgetc(ctx->file);
    if (ch == '-') {
        numbersBuffer[position++] = '-';
    } else {
        ungetc(ch, ctx->file);
    }

    while ((ch = fgetc(ctx->file)) != EOF && isDigit(ch)) {
        if (position >= DIGITS_IN_LONG) {
            reportError(ctx, "Encountered number exceeds max digits number (%d).", DIGITS_IN_LONG);
            return;
        }
        numbersBuffer[position++] = (char) ch;
    }

    ungetc(ch, ctx->file);
    numbersBuffer[position] = '\0';

    if (position == 0 || (position == 1 && numbersBuffer[0] == '-')) {
        reportError(ctx, "Expected a number, found '%c' (0x%02X).", ch, ch);
        return;
    }

    *resultNumber = strtol(numbersBuffer, NULL, 10);
}

BencodeNode *parseList(BencodeContext *ctx) {
    if (ctx->hasError) return NULL;

    const int peekedChar = fpeek(ctx->file);
    if (peekedChar == EOF) {
        reportError(ctx, "Unexpected EOF while parsing the list.");
        fgetc(ctx->file);
        return NULL;
    }

    BencodeNode *node = malloc(sizeof(BencodeNode));
    node->type = BEN_LIST;
    node->list.length = 0;
    node->list.capacity = 0;

    if (peekedChar == 'e') {
        node->list.items = NULL;
        fgetc(ctx->file);
        return node;
    }

    node->list.items = malloc(sizeof(BencodeNode *) * DEFAULT_COLLECTION_ITEMS);
    node->list.capacity = DEFAULT_COLLECTION_ITEMS;

    int ch;
    while ((ch = fpeek(ctx->file)) != EOF && ch != 'e') {
        BencodeNode *item = parseCollectionValue(ctx);

        if (ctx->hasError) {
            freeBencodeNode(node);
            return NULL;
        }
        if (node->list.length >= node->list.capacity) {
            node->list.capacity *= 2;
            node->list.items = realloc(node->list.items, sizeof(BencodeNode *) * node->list.capacity);
        }
        node->list.items[node->list.length++] = item;
    }

    if (ch == EOF) {
        freeBencodeNode(node);
        reportError(ctx, "Unexpected EOF while parsing the list.");
        return NULL;
    }

    fgetc(ctx->file);

    return node;
}

BencodeNode *parseDict(BencodeContext *ctx) {
    if (ctx->hasError) return NULL;

    const int peekedChar = fpeek(ctx->file);
    if (peekedChar == EOF) {
        reportError(ctx, "Unexpected EOF while parsing the dict.");
        fgetc(ctx->file);
        return NULL;
    }

    BencodeNode *node = malloc(sizeof(BencodeNode));
    node->type = BEN_DICT;
    node->dict.length = 0;
    node->dict.capacity = 0;

    if (peekedChar == 'e') {
        node->dict.keys = NULL;
        node->dict.values = NULL;
        fgetc(ctx->file);
        return node;
    }

    node->dict.keys = malloc(sizeof(char *) * DEFAULT_COLLECTION_ITEMS);
    node->dict.values = malloc(sizeof(BencodeNode *) * DEFAULT_COLLECTION_ITEMS);
    node->dict.capacity = DEFAULT_COLLECTION_ITEMS;

    int ch;
    while ((ch = fpeek(ctx->file)) != EOF && ch != 'e') {
        if (!isDigit(ch)) {
            reportError(ctx, "Expected digit while parsing key of the dictionary, encountered: %c.", ch);
            return NULL;
        }

        BencodeNode *key = parseString(ctx);
        if (ctx->hasError) {
            free(key);
            return NULL;
        }
        BencodeNode *value = parseCollectionValue(ctx);

        if (ctx->hasError) {
            freeBencodeNode(key);
            freeBencodeNode(value);
            return NULL;
        }

        node->dict.length++;
        if (node->dict.length > node->dict.capacity) {
            const size_t newCapacity = node->dict.capacity + DEFAULT_COLLECTION_ITEMS;
            node->dict.capacity = newCapacity;
            node->dict.keys = realloc(node->dict.keys, sizeof(char *) * newCapacity);
            node->dict.values = realloc(node->dict.values, sizeof(BencodeNode *) * newCapacity);
        }

        node->dict.keys[node->dict.length - 1] = (char *) key->string.data;
        node->dict.values[node->dict.length - 1] = value;
        free(key);
    }

    if (ch == EOF) {
        reportError(ctx, "Unexpected EOF while parsing the dict.");
    }
    fgetc(ctx->file);

    return node;
}

BencodeNode *parseString(BencodeContext *ctx) {
    if (ctx->hasError) return NULL;

    long charsToRead;
    extractNumber(ctx, &charsToRead);
    if (ctx->hasError) return NULL;
    // todo: handle negative numbers

    const int nextChar = fgetc(ctx->file);
    if (nextChar != ':') {
        reportError(ctx, "Expected ':' after string length, found '%c'.", nextChar);
        return NULL;
    }

    BencodeNode *node = malloc(sizeof(BencodeNode));
    node->type = BEN_STR;
    node->string.length = charsToRead;
    node->string.data = malloc(charsToRead);

    size_t readLength = fread(node->string.data, 1, charsToRead, ctx->file);
    if (readLength != charsToRead) {
        freeBencodeNode(node);
        reportError(ctx, "Unexpected EOF reading string data. Expected %ld, got %ld.", charsToRead, readLength);
        return NULL;
    }
    return node;
}

BencodeNode *parseInt(BencodeContext *ctx) {
    if (ctx->hasError) return NULL;

    BencodeNode *node = malloc(sizeof(BencodeNode));
    node->type = BEN_INT;

    long parsedNumber;
    extractNumber(ctx, &parsedNumber);
    node->intValue = parsedNumber;
    const int ch = getc(ctx->file);
    if (ch != 'e') {
        reportError(ctx, "Encountered '%c' instead of expected 'e' while parsing int.", ch);
    }
    return node;
}

BencodeNode *parseCollectionValue(BencodeContext *ctx) {
    const int ch = getc(ctx->file);
    switch (ch) {
        case 'l':
            return parseList(ctx);
        case 'd':
            return parseDict(ctx);
        case 'i':
            return parseInt(ctx);
        default:
            if (isDigit(ch)) {
                ungetc(ch, ctx->file);
                return parseString(ctx);
            }
    }
    return NULL;
}

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

int main() {
    char *fileName = "./../sometorrent.torrent";
    BencodeContext ctx;
    ctx.file = fopen(fileName, "rb");
    ctx.hasError = false;
    ctx.errorPosition = 0;
    memset(ctx.errorMsg, 0, sizeof(ctx.errorMsg));

    if (!ctx.file) {
        perror("Error opening file");
        return 1;
    }

    const int ch = fpeek(ctx.file);
    if (ch == EOF) {
        perror("File is empty.");
        return 1;
    }

    BencodeNode *root = NULL;

    if (ch == 'd') {
        fgetc(ctx.file);
        root = parseDict(&ctx);
    } else {
        reportError(&ctx, "File does not start with a dictionary (found '%c' instead).", ch);
    }

    if (ctx.hasError) {
        printf("Error: %s\n", ctx.errorMsg);
        printf("Position: %ld (0x%lX)\n", ctx.errorPosition, ctx.errorPosition);

        if (root) {
            freeBencodeNode(root);
        }
    } else {
        freeBencodeNode(root);
    }

    fclose(ctx.file);
    return 0;
}
