#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define COLUMN_CHAR 58 // :
#define DICT_CHAR 100 // d
#define END_CHAR 101 // e
#define INT_CHAR 105 // i
#define LIST_CHAR 108 // l

#define DIGITS_IN_LONG 19

typedef struct {
    const char *key;
    const char *value;
} DictionaryItem;

const DictionaryItem BencodeSymbols[] = {
    {"d", "e"}
};

const char *bencodeSymbols[] = {"d", "l"};

char getAsciiCharFromBinary(const char *const str) {
    int multiplier = 1;

    return (char) str[0];
}

typedef struct TorrentInfo {
    char *fileName;
    int fileSize;
} TorrentInfo;


struct Torrent {
    char *announce;
    char **announceList;
    int announceListSize;

    char *comment;
    char *createdBy;
    int creationDateTimestamp;
    TorrentInfo infos[];
} Torrent;

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

void getNumberFromTorrentFile(int* ch, FILE *const torrentFile, bool *isSuccess, long* number) {
    char numbersBuffer[DIGITS_IN_LONG + 1];
    int position = 0;
    numbersBuffer[position++] = (char) *ch;
    while ((*ch = fgetc(torrentFile)) != EOF && isDigit(*ch) && position < DIGITS_IN_LONG) {
        numbersBuffer[position++] = (char) *ch;
    }

    numbersBuffer[position] = '\0';

    if (position > DIGITS_IN_LONG) {
        *isSuccess = false;
        return;
    }

    *number = strtol(numbersBuffer, NULL, 10);
}

void handleDigit(int *ch, FILE *const torrentFile, bool *isSuccess) {
    long charsToRead;
    getNumberFromTorrentFile(ch, torrentFile, isSuccess, &charsToRead);

    if (!*isSuccess) {
        return;
    }
    if (*ch != COLUMN_CHAR) {
        *isSuccess = false;
        return;
    }

    char stringBuffer[charsToRead + 1];
    fread(stringBuffer, sizeof(char), charsToRead, torrentFile);
    stringBuffer[charsToRead] = '\0';
    printf("\nReading %ld chars \n", charsToRead);
    printf("%s", stringBuffer);
}

void handleInt(int *ch, FILE *const torrentFile, bool *isSuccess) {
    long parsedNumber;
    getNumberFromTorrentFile(ch, torrentFile, isSuccess, &parsedNumber);
    printf("\n%ld\n", parsedNumber);
}

void handleList(int *ch, FILE *const torrentFile, bool *isSuccess) {
    while ((*ch = fgetc(torrentFile)) != EOF && *ch != END_CHAR) {
        if (isDigit(*ch)) {
            handleDigit(ch, torrentFile, isSuccess);
            if (!*isSuccess) {
                return;
            }
        } else if (*ch == DICT_CHAR) {
            printf("OHHHHHHH DICT IN LIST");
            // handleDict(ch, torrentFile, isSuccess);
            // if (!*isSuccess) {
            //     return;
            // }
        } else if (*ch == INT_CHAR) {
            handleInt(ch, torrentFile, isSuccess);
            if (!*isSuccess) {
                return;
            }
        } else if (*ch == LIST_CHAR) {
            handleList(ch, torrentFile, isSuccess);
        } else {
            *isSuccess = false;
            return;
        }
    }

    if (*ch == EOF) {
        *isSuccess = false;
    }
}

void handleDict(int *ch, FILE *const torrentFile, bool *isSuccess) {
    while ((*ch = fgetc(torrentFile)) != EOF && *ch != END_CHAR) {
        if (isDigit(*ch)) {
            handleDigit(ch, torrentFile, isSuccess);
            if (!*isSuccess) {
                return;
            }
        } else if (*ch == DICT_CHAR) {
            handleDict(ch, torrentFile, isSuccess);
            if (!*isSuccess) {
                return;
            }
        } else if (*ch == INT_CHAR) {
            handleInt(ch, torrentFile, isSuccess);
            if (!*isSuccess) {
                return;
            }
        } else if (*ch == LIST_CHAR) {
            handleList(ch, torrentFile, isSuccess);
        } else {
            *isSuccess = false;
            return;
        }
    }

    if (*ch == EOF) {
        *isSuccess = false;
    }
}

int main() {
    FILE *torrentFile = fopen("./../sometorrent.torrent", "r");

    if (torrentFile == NULL) {
        exit(-1);
    }


    int ch = fgetc(torrentFile);
    if (ch == EOF) {
        fclose(torrentFile);
        exit(-1);
    }

    if (ch != DICT_CHAR) {
        fclose(torrentFile);
        exit(-1);
    }

    bool isSuccess = true;
    handleDict(&ch, torrentFile, &isSuccess);
    if (!isSuccess) {
        return -1;
    }
    if (ch == EOF) {
        fclose(torrentFile);
        exit(-1); // TODO: Error: dictionary was not closed by e
    }

    fclose(torrentFile);
}