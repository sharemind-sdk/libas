#include "tokens.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../likely.h"
#include "stdion.h"


SVM_ENUM_DEFINE_TOSTRING(SMA_TokenType, SMA_ENUM_TokenType);

static uint64_t read_hex(const char * c, size_t l) {
    const char * e = c + l;
    uint64_t v = 0u;
    do {
        char base;
        switch (*c) {
            case '0' ... '9': base = '0'; break;
            case 'a' ... 'f': base = 'a'; break;
            case 'A' ... 'F': base = 'A'; break;
            default:
                abort();
        }
        v = (v * 16) + (*c - base);
    } while (++c < e);
    return v;
}

uint64_t SMA_token_hex_value(const struct SMA_Token * t) {
    assert(t);
    assert(t->type == SMA_TOKEN_HEX);
    assert(t->length > 0u);
    size_t i;
    if (t->text[0] == '-') {
        assert(t->length >= 4u);
        assert(t->length <= 19u);
        assert(t->text[1] == '0');
        assert(t->text[2] == 'x');
        assert(t->length != 19u
               || t->text[3] == '0' || t->text[3] == '1' || t->text[3] == '2' || t->text[3] == '3'
               || t->text[3] == '4' || t->text[3] == '5' || t->text[3] == '6' || t->text[3] == '7');
        i = 3;
    } else {
        assert(t->length >= 3u);
        assert(t->length <= 18u);
        assert(t->text[0] == '0');
        assert(t->text[1] == 'x');
        i = 2;
    }

    uint64_t v = read_hex(t->text + i, t->length - i);

    if (t->text[0] != '-')
        return v;

    union { int64_t s; uint64_t u; } x = { .s = -v };
    return x.u;
}

size_t SMA_token_string_length(const struct SMA_Token * t) {
    assert(t);
    assert(t->type == SMA_TOKEN_STRING);
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

char * SMA_token_string_value(const struct SMA_Token * t, size_t * length) {
    assert(t);
    assert(t->type == SMA_TOKEN_STRING);
    assert(t->length >= 2u);
    size_t l = SMA_token_string_length(t);
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

char * SMA_token_label_label_new(const struct SMA_Token *t) {
    assert(t);
    assert(t->type == SMA_TOKEN_LABEL || t->type == SMA_TOKEN_LABEL_O);
    size_t l;
    if (t->type == SMA_TOKEN_LABEL) {
        assert(t->length >= 2u);
        l = t->length;
    } else {
        assert(t->length >= 6u);
        for (l = 2; t->text[l] != '+'; l++) /* Do nothing */;
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

uint64_t SMA_token_label_offset(const struct SMA_Token *t, int * negative) {
    assert(t);
    assert(t->type == SMA_TOKEN_LABEL || t->type == SMA_TOKEN_LABEL_O);
    assert(t->text[0] == ':');
    if (t->type == SMA_TOKEN_LABEL) {
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
        return read_hex(h, t->length - (h - t->text));
    }
}

struct SMA_Tokens * SMA_tokens_new() {
    struct SMA_Tokens * ts = malloc(sizeof(struct SMA_Tokens));
    if (unlikely(!ts))
        return NULL;
    ts->numTokens = 0u;
    ts->array = NULL;
    return ts;
}

void SMA_tokens_free(struct SMA_Tokens * ts) {
    free(ts->array);
    free(ts);
}

void SMA_tokens_print(const struct SMA_Tokens *ts) {
    assert(ts);
    int newLine = 1;
    for (size_t i = 0; i < ts->numTokens; i++) {
        struct SMA_Token * t = &ts->array[i];
        if (!newLine)
            putchar(' ');
        else
            newLine = 0;

        printf("%s", SMA_TokenType_toString(t->type) + 10u);
        switch (t->type) {
            case SMA_TOKEN_NEWLINE:
                putchar('\n');
                newLine = 1;
                break;
            case SMA_TOKEN_DIRECTIVE:
            case SMA_TOKEN_HEX:
            case SMA_TOKEN_STRING:
            case SMA_TOKEN_LABEL_O:
            case SMA_TOKEN_LABEL:
            case SMA_TOKEN_KEYWORD:
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

struct SMA_Token * SMA_tokens_append(struct SMA_Tokens * ts,
                                     enum SMA_TokenType type,
                                     const char * start,
                                     size_t start_line,
                                     size_t start_column)
{
    assert(ts);
    struct SMA_Token * nts = realloc(ts->array, sizeof(struct SMA_Token) * (ts->numTokens + 1));
    if (unlikely(!nts))
        return NULL;
    ts->array = nts;

    struct SMA_Token * nt = &nts[ts->numTokens];
    ts->numTokens++;
    nt->type = type;
    nt->text = start;
    nt->start_line = start_line;
    nt->start_column = start_column;
    return nt;
}

void SMA_tokens_pop_back_newlines(struct SMA_Tokens * ts) {
    assert(ts);

    if (ts->array[ts->numTokens - 1].type != SMA_TOKEN_NEWLINE)
        return;

    ts->numTokens--;
    struct SMA_Token * nts = realloc(ts->array, sizeof(struct SMA_Token) * ts->numTokens);
    if (likely(nts))
        ts->array = nts;
}
