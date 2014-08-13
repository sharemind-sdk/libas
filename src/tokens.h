/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBAS_TOKENS_H
#define SHAREMIND_LIBAS_TOKENS_H

#include <sharemind/preprocessor.h>
#include <stdint.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif


uint64_t sharemind_assembler_read_hex(const char * c, size_t l) __attribute__ ((nonnull(1)));

#define SHAREMIND_ASSEMBLER_TOKEN_TYPE_ENUM \
    (SHAREMIND_ASSEMBLER_TOKEN_NEWLINE) \
    (SHAREMIND_ASSEMBLER_TOKEN_DIRECTIVE) \
    (SHAREMIND_ASSEMBLER_TOKEN_HEX) \
    (SHAREMIND_ASSEMBLER_TOKEN_UHEX) \
    (SHAREMIND_ASSEMBLER_TOKEN_STRING) \
    (SHAREMIND_ASSEMBLER_TOKEN_LABEL_O) \
    (SHAREMIND_ASSEMBLER_TOKEN_LABEL) \
    (SHAREMIND_ASSEMBLER_TOKEN_KEYWORD)
SHAREMIND_ENUM_DEFINE(SharemindAssemblerTokenType, SHAREMIND_ASSEMBLER_TOKEN_TYPE_ENUM);
SHAREMIND_ENUM_DECLARE_TOSTRING(SharemindAssemblerTokenType);

typedef struct {
    SharemindAssemblerTokenType type;
    const char * text;
    size_t start_line;
    size_t start_column;
    size_t length;
} SharemindAssemblerToken;

int64_t SharemindAssemblerToken_hex_value(const SharemindAssemblerToken * t) __attribute__ ((nonnull(1)));
uint64_t SharemindAssemblerToken_uhex_value(const SharemindAssemblerToken * t) __attribute__ ((nonnull(1)));

size_t SharemindAssemblerToken_string_length(const SharemindAssemblerToken * t) __attribute__ ((nonnull(1)));
char * SharemindAssemblerToken_string_value(const SharemindAssemblerToken * t, size_t * length) __attribute__ ((nonnull(1)));

char * SharemindAssemblerToken_label_to_new_string(const SharemindAssemblerToken * t) __attribute__ ((nonnull(1), warn_unused_result));
int64_t SharemindAssemblerToken_label_offset(const SharemindAssemblerToken * t) __attribute__ ((nonnull(1)));

typedef struct {
    size_t numTokens;
    SharemindAssemblerToken * array;
} SharemindAssemblerTokens;

SharemindAssemblerTokens * SharemindAssemblerTokens_new(void) __attribute__ ((warn_unused_result));

void SharemindAssemblerTokens_free(SharemindAssemblerTokens * ts) __attribute__ ((nonnull(1)));

void SharemindAssemblerTokens_print(const SharemindAssemblerTokens * ts) __attribute__ ((nonnull(1)));

SharemindAssemblerToken * SharemindAssemblerTokens_append(
        SharemindAssemblerTokens * ts,
        SharemindAssemblerTokenType type,
        const char * start,
        size_t start_line,
        size_t start_column)
    __attribute__ ((nonnull(1, 3)));

void SharemindAssemblerTokens_pop_back_newlines(SharemindAssemblerTokens * ts) __attribute__ ((nonnull(1)));


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBAS_TOKENS_H */
