/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMA_TOKENS_H
#define LIBSMA_TOKENS_H

#include <stdint.h>
#include <string.h>
#include "../preprocessor.h"


#ifdef __cplusplus
extern "C" {
#endif

#define SMA_ENUM_TokenType \
    (SMA_TOKEN_NEWLINE) \
    (SMA_TOKEN_DIRECTIVE) \
    (SMA_TOKEN_HEX) \
    (SMA_TOKEN_STRING) \
    (SMA_TOKEN_LABEL_O) \
    (SMA_TOKEN_LABEL) \
    (SMA_TOKEN_KEYWORD)
SVM_ENUM_DEFINE(SMA_TokenType, SMA_ENUM_TokenType);
SVM_ENUM_DECLARE_TOSTRING(SMA_TokenType);

struct SMA_Token {
    enum SMA_TokenType type;
    const char * text;
    size_t start_line;
    size_t start_column;
    size_t length;
};

uint64_t SMA_token_hex_value(const struct SMA_Token * t);

size_t SMA_token_string_length(const struct SMA_Token * t);
char * SMA_token_string_value(const struct SMA_Token * t, size_t * length);

char * SMA_token_label_label_new(const struct SMA_Token *t);
uint64_t SMA_token_label_offset(const struct SMA_Token *t, int * negative);

struct SMA_Tokens {
    size_t numTokens;
    struct SMA_Token * array;
};

struct SMA_Tokens * SMA_tokens_new();

void SMA_tokens_free(struct SMA_Tokens * ts);

void SMA_tokens_print(const struct SMA_Tokens *ts);

struct SMA_Token * SMA_tokens_append(struct SMA_Tokens * ts,
                                     enum SMA_TokenType type,
                                     const char * start,
                                     size_t start_line,
                                     size_t start_column);

void SMA_tokens_pop_back_newlines(struct SMA_Tokens * ts);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMA_TOKENS_H */
