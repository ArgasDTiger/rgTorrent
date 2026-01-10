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
    const char* key;
    const char* value;
} DictionaryItem;

const DictionaryItem BencodeSymbols[] = {
    {"d", "e"}
};

const char* bencodeSymbols[] = {"d", "l"};

char getAsciiCharFromBinary(const char* const str) {
    int multiplier = 1;

    return (char)str[0];
}

typedef struct TorrentInfo {
    char* fileName;
    int fileSize;
} TorrentInfo;


struct Torrent {
    char* announce;
    char** announceList;
    int announceListSize;

    char* comment;
    char* createdBy;
    int creationDateTimestamp;
    TorrentInfo infos[];
} Torrent;

bool isDigit(const int ch) {
    return ch >= 48 && ch <= 57;
}

int fpeek(FILE * const fp) {
    const int c = fgetc(fp);
    if (c != EOF) {
        ungetc(c, fp);
    }
    return c;
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

    while ((ch = fgetc(torrentFile)) != EOF && ch != END_CHAR) {
        if (isDigit(ch)) {
            char numbersBuffer[DIGITS_IN_LONG + 1];
            int position = 0;
            numbersBuffer[position++] = (char)ch;
            while ((ch = fgetc(torrentFile)) != EOF && isDigit(ch) && position < DIGITS_IN_LONG) {
                numbersBuffer[position++] = (char)ch;
            }

            numbersBuffer[position] = '\0';

            if (ch != COLUMN_CHAR || position > DIGITS_IN_LONG) {
                fclose(torrentFile);
                exit(-1);
            }

            const long charsToRead = strtol(numbersBuffer, NULL, 10);
            char stringBuffer[charsToRead + 1];
            fread(stringBuffer, sizeof(char), charsToRead, torrentFile);
            stringBuffer[charsToRead] = '\0';
            printf("\n Reading %ld chars \n", charsToRead);
            printf("%s", stringBuffer);
        } else if (ch == DICT_CHAR) {
        } else if (ch == INT_CHAR) {
        } else if (ch == LIST_CHAR) {
        } else {
            exit(-1);
        }
    }

    if (ch == EOF) {
        fclose(torrentFile);
        exit(-1); // TODO: Error: dictionary was not closed by e
    }

    fclose(torrentFile);
}
