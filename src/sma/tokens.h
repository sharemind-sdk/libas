#ifndef TOKENS_H
#define TOKENS_H

#include <stdint.h>
#include <string.h>
#include "../preprocessor.h"


#ifdef __cplusplus
extern "C" {
#endif

#define SMA_ENUM_TokenType \
    (TOKEN_START) \
    (TOKEN_BOM) \
    (TOKEN_WHITESPACE) \
    (TOKEN_COMMENT) \
    (TOKEN_NEWLINE) \
    (TOKEN_EOF) \
    (TOKEN_DIRECTIVE) \
    (TOKEN_HEX) \
    (TOKEN_STRING) \
    (TOKEN_LABEL_O) \
    (TOKEN_LABEL) \
    (TOKEN_KEYWORD)
SVM_ENUM_DEFINE(SMA_TokenType, SMA_ENUM_TokenType);
SVM_ENUM_DECLARE_TOSTRING(SMA_TokenType);

struct Token {
    enum SMA_TokenType type;
    const char * text;
    size_t start_line;
    size_t start_column;
    size_t length;
};

uint64_t token_hex_value(const struct Token * t);

size_t token_string_length(const struct Token * t);
char * token_string_value(const struct Token * t, size_t * length);

struct Tokens {
    size_t numTokens;
    struct Token * array;
};

struct Tokens * tokens_new();

void tokens_print(const struct Tokens *ts);

struct Token * tokens_append(struct Tokens * ts, enum SMA_TokenType type,
                             const char * start, size_t start_line, size_t start_column);

void tokens_pop_back_newlines(struct Tokens * ts);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* TOKENS_H */
