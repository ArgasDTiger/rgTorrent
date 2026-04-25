#ifndef BENCODE_PARSER_H
#define BENCODE_PARSER_H
#include "bencoder.h"

BencodeNode *parseList(BencodeContext *ctx);

BencodeNode *parseDict(BencodeContext *ctx);

BencodeNode *parseString(BencodeContext *ctx);

BencodeNode *parseInt(BencodeContext *ctx);

BencodeNode *parseCollectionValue(BencodeContext *ctx);

#endif
