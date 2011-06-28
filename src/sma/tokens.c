#include "tokens.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../likely.h"
#include "stdion.h"


SVM_ENUM_DEFINE_TOSTRING(SMA_TokenType, SMA_ENUM_TokenType);

uint64_t token_hex_value(const struct Token * t) {
    assert(t);
    assert(t->type == TOKEN_HEX);
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

    uint64_t v = 0u;
    do {
        char base;
        switch (t->text[i]) {
            case '0' ... '9': base = '0'; break;
            case 'a' ... 'f': base = 'a'; break;
            case 'A' ... 'F': base = 'A'; break;
            default:
                abort();
        }
        v = (v * 16) + (t->text[i] - base);
    } while (++i < t->length);

    if (t->text[0] != '-')
        return v;

    union { int64_t s; uint64_t u; } x = { .s = -v };
    return x.u;
}

size_t token_string_length(const struct Token * t) {
    assert(t);
    assert(t->type == TOKEN_STRING);
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

char * token_string_value(const struct Token * t, size_t * length) {
    assert(t);
    assert(t->type == TOKEN_STRING);
    assert(t->length >= 2u);
    size_t l = token_string_length(t);
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


struct Tokens * tokens_new() {
    struct Tokens * ts = malloc(sizeof(struct Tokens));
    if (unlikely(!ts))
        return NULL;
    ts->numTokens = 0u;
    ts->array = NULL;
    return ts;
}

void tokens_print(const struct Tokens *ts) {
    assert(ts);
    int newLine = 1;
    for (size_t i = 0; i < ts->numTokens; i++) {
        struct Token * t = &ts->array[i];
        if (!newLine)
            putchar(' ');
        else
            newLine = 0;

        printf("%s", SMA_TokenType_toString(t->type) + 6);
        switch (t->type) {
            case TOKEN_NEWLINE:
                putchar('\n');
                newLine = 1;
                break;
            case TOKEN_DIRECTIVE:
            case TOKEN_HEX:
            case TOKEN_STRING:
            case TOKEN_LABEL_O:
            case TOKEN_LABEL:
            case TOKEN_KEYWORD:
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

struct Token * tokens_append(struct Tokens * ts, enum SMA_TokenType type,
                             const char * start, size_t start_line, size_t start_column)
{
    assert(ts);
    struct Token * nts = realloc(ts->array, sizeof(struct Token) * (ts->numTokens + 1));
    if (unlikely(!nts))
        return NULL;
    ts->array = nts;

    struct Token * nt = &nts[ts->numTokens];
    ts->numTokens++;
    nt->type = type;
    nt->text = start;
    nt->start_line = start_line;
    nt->start_column = start_column;
    return nt;
}

void tokens_pop_back_newlines(struct Tokens * ts) {
    assert(ts);

    if (ts->array[ts->numTokens - 1].type != TOKEN_NEWLINE)
        return;

    ts->numTokens--;
    struct Token * nts = realloc(ts->array, sizeof(struct Token) * ts->numTokens);
    if (likely(nts))
        ts->array = nts;
}
