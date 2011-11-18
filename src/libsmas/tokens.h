/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMAS_TOKENS_H
#define SHAREMIND_LIBSMAS_TOKENS_H

#include <stdint.h>
#include <string.h>
#include "../preprocessor.h"


#ifdef __cplusplus
extern "C" {
#endif

uint64_t SMAS_read_hex(const char * c, size_t l);

#define SMAS_ENUM_TokenType \
    (SMAS_TOKEN_NEWLINE) \
    (SMAS_TOKEN_DIRECTIVE) \
    (SMAS_TOKEN_HEX) \
    (SMAS_TOKEN_UHEX) \
    (SMAS_TOKEN_STRING) \
    (SMAS_TOKEN_LABEL_O) \
    (SMAS_TOKEN_LABEL) \
    (SMAS_TOKEN_KEYWORD)
SM_ENUM_DEFINE(SMAS_TokenType, SMAS_ENUM_TokenType);
SM_ENUM_DECLARE_TOSTRING(SMAS_TokenType);

typedef struct {
    SMAS_TokenType type;
    const char * text;
    size_t start_line;
    size_t start_column;
    size_t length;
} SMAS_Token;

int64_t SMAS_token_hex_value(const SMAS_Token * t);
uint64_t SMAS_token_uhex_value(const SMAS_Token * t);

size_t SMAS_token_string_length(const SMAS_Token * t);
char * SMAS_token_string_value(const SMAS_Token * t, size_t * length);

char * SMAS_token_label_label_new(const SMAS_Token *t);
int64_t SMAS_token_label_offset(const SMAS_Token *t);

typedef struct {
    size_t numTokens;
    SMAS_Token * array;
} SMAS_Tokens;

SMAS_Tokens * SMAS_tokens_new(void);

void SMAS_tokens_free(SMAS_Tokens * ts);

void SMAS_tokens_print(const SMAS_Tokens *ts);

SMAS_Token * SMAS_tokens_append(SMAS_Tokens * ts,
                                SMAS_TokenType type,
                                const char * start,
                                size_t start_line,
                                size_t start_column);

void SMAS_tokens_pop_back_newlines(SMAS_Tokens * ts);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMAS_TOKENS_H */
