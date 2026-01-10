#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

BencodeNode *parseList(FILE *torrentFile, bool *isSuccess);

BencodeNode *parseDict(FILE *torrentFile, bool *isSuccess);

BencodeNode *parseString(FILE *torrentFile, bool *isSuccess);

BencodeNode *parseInt(FILE *torrentFile, bool *isSuccess);

void freeBencodeNode(BencodeNode* node);

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

void extractNumber(FILE *const torrentFile, bool *isSuccess, long *resultNumber) {
    char numbersBuffer[DIGITS_IN_LONG + 1];
    int ch, position = 0;
    while ((ch = fgetc(torrentFile)) != EOF && isDigit(ch) && position < DIGITS_IN_LONG) {
        numbersBuffer[position++] = (char) ch;
    }

    ungetc(ch, torrentFile);

    numbersBuffer[position] = '\0';

    if (position > DIGITS_IN_LONG) {
        *isSuccess = false;
        return;
    }
    *resultNumber = strtol(numbersBuffer, NULL, 10);
}

BencodeNode *parseString(FILE *const torrentFile, bool *isSuccess) {
    long charsToRead;
    extractNumber(torrentFile, isSuccess, &charsToRead);
    const int nextChar = getc(torrentFile);
    if (!*isSuccess || nextChar != ':') {
        return NULL;
    }

    BencodeNode *node = malloc(sizeof(BencodeNode));
    node->type = BEN_STR;
    node->string.length = charsToRead;
    node->string.data = malloc(charsToRead);

    fread(node->string.data, sizeof(char), charsToRead, torrentFile);
    return node;
}

BencodeNode *parseInt(FILE *const torrentFile, bool *isSuccess) {
    BencodeNode *node = malloc(sizeof(BencodeNode));
    node->type = BEN_INT;

    long parsedNumber;
    extractNumber(torrentFile, isSuccess, &parsedNumber);
    node->intValue = parsedNumber;
    if (getc(torrentFile) != 'e') {
        *isSuccess = false;
    }
    return node;
}

BencodeNode *parseCollectionValue(FILE *const torrentFile, bool *isSuccess) {
    const int ch = getc(torrentFile);
    switch (ch) {
        case 'l':
            return parseList(torrentFile, isSuccess);
        case 'd':
            return parseDict(torrentFile, isSuccess);
        case 'i':
            return parseInt(torrentFile, isSuccess);
        default:
            if (isDigit(ch)) {
                ungetc(ch, torrentFile);
                return parseString(torrentFile, isSuccess);
            }
    }
    return NULL;
}

BencodeNode *parseList(FILE *const torrentFile, bool *isSuccess) {
    const int peekedChar = fpeek(torrentFile);
    if (peekedChar == EOF) {
        *isSuccess = false;
        fgetc(torrentFile);
        return NULL;
    }

    BencodeNode *node = malloc(sizeof(BencodeNode));
    node->type = BEN_LIST;
    node->list.length = 0;
    node->list.capacity = 0;

    if (peekedChar == 'e') {
        node->list.items = NULL;
        fgetc(torrentFile);
        return node;
    }

    node->list.items = malloc(sizeof(BencodeNode *) * DEFAULT_COLLECTION_ITEMS);
    node->list.capacity = DEFAULT_COLLECTION_ITEMS;

    int ch;
    while ((ch = fpeek(torrentFile)) != EOF && ch != 'e') {
        BencodeNode *item = parseCollectionValue(torrentFile, isSuccess);

        if (!*isSuccess) {
            freeBencodeNode(node);
            return NULL;
        }

        if (node->list.length + 1 > node->list.capacity) {
            const size_t newCapacity = node->list.length + DEFAULT_COLLECTION_ITEMS;
            node->list.capacity = newCapacity;
            node->list.items = realloc(node->list.items, sizeof(BencodeNode *) * newCapacity);
        }

        node->list.items[node->list.length++] = item;
    }

    if (ch == EOF) {
        *isSuccess = false;
    }
    fgetc(torrentFile);

    return node;
}

BencodeNode *parseDict(FILE *const torrentFile, bool *isSuccess) {
    const int peekedChar = fpeek(torrentFile);
    if (peekedChar == EOF) {
        *isSuccess = false;
        fgetc(torrentFile);
        return NULL;
    }

    BencodeNode *node = malloc(sizeof(BencodeNode));
    node->type = BEN_DICT;
    node->dict.length = 0;
    node->dict.capacity = 0;

    if (peekedChar == 'e') {
        node->dict.keys = NULL;
        node->dict.values = NULL;
        fgetc(torrentFile);
        return node;
    }

    node->dict.keys = malloc(sizeof(char *) * DEFAULT_COLLECTION_ITEMS);
    node->dict.values = malloc(sizeof(BencodeNode *) * DEFAULT_COLLECTION_ITEMS);
    node->dict.capacity = DEFAULT_COLLECTION_ITEMS;

    int ch;
    while ((ch = fpeek(torrentFile)) != EOF && ch != 'e') {
        if (!isDigit(ch)) {
            *isSuccess = false;
            return NULL;
        }

        BencodeNode *key = parseString(torrentFile, isSuccess);
        if (!*isSuccess) {
            free(key);
            return NULL;
        }
        BencodeNode *value = parseCollectionValue(torrentFile, isSuccess);

        if (!*isSuccess) {
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

        node->dict.keys[node->dict.length - 1] = (char*)key->string.data;
        node->dict.values[node->dict.length - 1] = value;
        free(key);
    }

    if (ch == EOF) {
        *isSuccess = false;
    }
    fgetc(torrentFile);

    return node;
}

void freeBencodeNode(BencodeNode* node) {
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

int main() {
    FILE *torrentFile = fopen("./../sometorrent.torrent", "rb");

    if (torrentFile == NULL) {
        exit(-1);
    }


    const int ch = fgetc(torrentFile);
    if (ch == EOF) {
        fclose(torrentFile);
        exit(-1);
    }

    if (ch != 'd') {
        fclose(torrentFile);
        exit(-1);
    }

    bool isSuccess = true;
    BencodeNode *root = parseDict(torrentFile, &isSuccess);
    if (!isSuccess) {
        fclose(torrentFile);
        free(root);
        return -1;
    }

    freeBencodeNode(root);
    fclose(torrentFile);
    return 0;
}
