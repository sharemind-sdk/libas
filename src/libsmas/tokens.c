/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "tokens.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../likely.h"
#include "stdion.h"


SM_ENUM_DEFINE_TOSTRING(SMAS_TokenType, SMAS_ENUM_TokenType);

uint64_t SMAS_read_hex(const char * c, size_t l) {
    const char * e = c + l;
    uint64_t v = 0u;
    do {
        char base;
        switch (*c) {
            case '0' ... '9': base = '0'; break;
            case 'a' ... 'f': base = 'a' - 10; break;
            case 'A' ... 'F': base = 'A' - 10; break;
            default:
                abort();
        }
        v = (v * 16) + (*c - base);
    } while (++c < e);
    return v;
}

uint64_t SMAS_token_hex_value(const struct SMAS_Token * t) {
    assert(t);
    assert(t->type == SMAS_TOKEN_HEX);
    assert(t->length > 0u);

    size_t i = 0u;
    if (t->text[0] == '-' || t->text[0] == '+')
        i++;

    assert(t->length >= 3u + i);
    assert(t->length <= 18u + i);
    assert(t->text[i] == '0');
    assert(t->text[i + 1] == 'x');
    i += 2u;

    uint64_t v = SMAS_read_hex(t->text + i, t->length - i);

    if (t->text[0] != '-')
        return v;

    union { int64_t s; uint64_t u; } x = { .s = -v };
    return x.u;
}

size_t SMAS_token_string_length(const struct SMAS_Token * t) {
    assert(t);
    assert(t->type == SMAS_TOKEN_STRING);
    assert(t->length >= 2u);
    size_t l = 0u;
    for (size_t i = 1u; i < t->length - 1; i++) {
        l++;
        if (t->text[i] == '\\') {
            /** \todo \xFFFF.. and \377 syntax. */
            i++;
            assert(i < t->length);
        }
    }
    return l;
}

char * SMAS_token_string_value(const struct SMAS_Token * t, size_t * length) {
    assert(t);
    assert(t->type == SMAS_TOKEN_STRING);
    assert(t->length >= 2u);
    size_t l = SMAS_token_string_length(t);
    if (length)
        *length = l;

    char * s = malloc(sizeof(char) * (l + 1));
    if (unlikely(!s))
        return NULL;

    char * op = s;
    const char * ip = &t->text[1];
    const char * ip_end = &t->text[t->length - 1u];

    while (ip != ip_end) {
        if (*ip != '\\') {
            *op = *ip;
        } else {
            ip++;
            assert(ip != ip_end);
            switch (*ip) {
                case 'n':  *op = '\n'; break;
                case 'r':  *op = '\r'; break;
                case 't':  *op = '\t'; break;
                case 'v':  *op = '\v'; break;
                case 'b':  *op = '\b'; break;
                case 'f':  *op = '\f'; break;
                case 'a':  *op = '\a'; break;
                case '0':  *op = '\0'; break; /**< \todo proper \xFFFF.. and \377 syntax. */
                case '\'': /* *op = '\''; break; */
                case '"':  /* *op = '"';  break; */
                case '?':  /* *op = '?';  break; */
                case '\\': /* *op = '\\'; break; */
                default:   *op = *ip;  break;
            }
        }
        ip++;
        op++;
    }

    s[l] = '\0';
    return s;
}

char * SMAS_token_label_label_new(const struct SMAS_Token *t) {
    assert(t);
    assert(t->type == SMAS_TOKEN_LABEL || t->type == SMAS_TOKEN_LABEL_O);
    size_t l;
    if (t->type == SMAS_TOKEN_LABEL) {
        assert(t->length >= 2u);
        l = t->length;
    } else {
        assert(t->length >= 6u);
        for (l = 2; t->text[l] != '+' && t->text[l] != '-'; l++) /* Do nothing */;
        assert(t->text[l + 1] == '0');
        assert(t->text[l + 2] == 'x');
    }

    char * c = malloc(l);
    if (!c)
        return NULL;

    l--;
    strncpy(c, t->text + 1, l);
    c[l] = '\x00';
    return c;
}

uint64_t SMAS_token_label_offset(const struct SMAS_Token *t, int * negative) {
    assert(t);
    assert(t->type == SMAS_TOKEN_LABEL || t->type == SMAS_TOKEN_LABEL_O);
    assert(t->text[0] == ':');
    if (t->type == SMAS_TOKEN_LABEL) {
        assert(t->length >= 2u);
        *negative = 0;
        return 0;
    } else {
        assert(t->length >= 6u);
        const char * h = t->text + 2;
        for (;; h++) {
            if (*h == '+') {
                *negative = 0;
                break;
            } else if (*h == '-') {
                *negative = 1;
                break;
            }
        }
        h += 3;
        return SMAS_read_hex(h, t->length - (h - t->text));
    }
}

struct SMAS_Tokens * SMAS_tokens_new() {
    struct SMAS_Tokens * ts = malloc(sizeof(struct SMAS_Tokens));
    if (unlikely(!ts))
        return NULL;
    ts->numTokens = 0u;
    ts->array = NULL;
    return ts;
}

void SMAS_tokens_free(struct SMAS_Tokens * ts) {
    free(ts->array);
    free(ts);
}

void SMAS_tokens_print(const struct SMAS_Tokens *ts) {
    assert(ts);
    int newLine = 1;
    for (size_t i = 0; i < ts->numTokens; i++) {
        struct SMAS_Token * t = &ts->array[i];
        if (!newLine)
            putchar(' ');
        else
            newLine = 0;

        const char * tokenStr = SMAS_TokenType_toString(t->type);
        assert(tokenStr);
        printf("%s", tokenStr + 10u);
        switch (t->type) {
            case SMAS_TOKEN_NEWLINE:
                putchar('\n');
                newLine = 1;
                break;
            case SMAS_TOKEN_DIRECTIVE:
            case SMAS_TOKEN_HEX:
            case SMAS_TOKEN_STRING:
            case SMAS_TOKEN_LABEL_O:
            case SMAS_TOKEN_LABEL:
            case SMAS_TOKEN_KEYWORD:
                putchar('(');
                nputs(t->text, t->length);
                putchar(')');
                break;
            default:
                break;
        }
    }
    printf("\n");
}

struct SMAS_Token * SMAS_tokens_append(struct SMAS_Tokens * ts,
                                       enum SMAS_TokenType type,
                                       const char * start,
                                       size_t start_line,
                                       size_t start_column)
{
    assert(ts);
    struct SMAS_Token * nts = realloc(ts->array, sizeof(struct SMAS_Token) * (ts->numTokens + 1));
    if (unlikely(!nts))
        return NULL;
    ts->array = nts;

    struct SMAS_Token * nt = &nts[ts->numTokens];
    ts->numTokens++;
    nt->type = type;
    nt->text = start;
    nt->start_line = start_line;
    nt->start_column = start_column;
    return nt;
}

void SMAS_tokens_pop_back_newlines(struct SMAS_Tokens * ts) {
    assert(ts);

    if (ts->array[ts->numTokens - 1].type != SMAS_TOKEN_NEWLINE)
        return;

    ts->numTokens--;
    struct SMAS_Token * nts = realloc(ts->array, sizeof(struct SMAS_Token) * ts->numTokens);
    if (likely(nts))
        ts->array = nts;
}
