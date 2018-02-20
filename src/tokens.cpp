/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#include "tokens.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sharemind/abort.h>
#include <sharemind/likely.h>
#include <sharemind/null.h>


SHAREMIND_ENUM_DEFINE_TOSTRING(SharemindAssemblerTokenType,
                               SHAREMIND_ASSEMBLER_TOKEN_TYPE_ENUM)

uint64_t sharemind_assembler_read_hex(const char * c, size_t l) {
    assert(c);

    const char * e = c + l;
    uint64_t v = 0u;
    do {
        uint64_t digit;
        switch (*c) {
            #define C(c,v) case c : digit = v; break
            C('0', 0u); C('1', 1u); C('2', 2u); C('3', 3u); C('4', 4u);
            C('5', 5u); C('6', 6u); C('7', 7u); C('8', 8u); C('9', 9u);
            C('a', 10u); C('b', 11u); C('c', 12u); C('d', 13u); C('e', 14u);
                C('f', 15u);
            C('A', 10u); C('B', 11u); C('C', 12u); C('D', 13u); C('E', 14u);
                C('F', 15u);
            #undef C
            default:
                abort();
        }
        v = (v * 16u) + digit;
    } while (++c < e);
    return v;
}

int64_t SharemindAssemblerToken_hex_value(const SharemindAssemblerToken * t) {
    assert(t);
    assert(t->type == SHAREMIND_ASSEMBLER_TOKEN_HEX);
    assert(t->length >= 4u);
    assert(t->length <= 19u);
    assert(t->text[0u] == '-' || t->text[0u] == '+');
    assert(t->text[1u] == '0');
    assert(t->text[2u] == 'x');

    uint64_t v = sharemind_assembler_read_hex(t->text + 3u, t->length - 3u);

    if (t->text[0] == '-') {
        assert(v <= ((uint64_t) (INT64_MIN + 1)) + 1u);
        return (int64_t) -v;
    } else {
        assert(t->text[0] == '+');
        assert(v <= (uint64_t) INT64_MAX);
        return (int64_t) v;
    }
}

uint64_t SharemindAssemblerToken_uhex_value(const SharemindAssemblerToken * t) {
    assert(t);
    assert(t->type == SHAREMIND_ASSEMBLER_TOKEN_UHEX);
    assert(t->length >= 3u);
    assert(t->length <= 18u);
    assert(t->text[0u] == '0');
    assert(t->text[1u] == 'x');

    return sharemind_assembler_read_hex(t->text + 2u, t->length - 2u);
}

size_t SharemindAssemblerToken_string_length(const SharemindAssemblerToken * t) {
    assert(t);
    assert(t->type == SHAREMIND_ASSEMBLER_TOKEN_STRING);
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

char * SharemindAssemblerToken_string_value(const SharemindAssemblerToken * t, size_t * length) {
    assert(t);
    assert(t->type == SHAREMIND_ASSEMBLER_TOKEN_STRING);
    assert(t->length >= 2u);
    size_t l = SharemindAssemblerToken_string_length(t);
    if (length)
        *length = l;

    char * s = (char *) malloc(sizeof(char) * (l + 1));
    if (unlikely(!s))
        return SHAREMIND_NULL;

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

char * SharemindAssemblerToken_label_to_new_string(const SharemindAssemblerToken * t) {
    assert(t);
    assert(t->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL || t->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL_O);
    size_t l;
    if (t->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL) {
        assert(t->length >= 2u);
        l = t->length;
    } else {
        assert(t->length >= 6u);
        for (l = 2; t->text[l] != '+' && t->text[l] != '-'; l++) /* Do nothing */;
        assert(t->text[l + 1] == '0');
        assert(t->text[l + 2] == 'x');
    }

    char * c = (char *) malloc(l);
    if (!c)
        return SHAREMIND_NULL;

    l--;
    strncpy(c, t->text + 1, l);
    c[l] = '\x00';
    return c;
}

int64_t SharemindAssemblerToken_label_offset(const SharemindAssemblerToken * t) {
    assert(t);
    assert(t->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL || t->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL_O);
    assert(t->text[0] == ':');
    if (t->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL) {
        assert(t->length >= 2u);
        return 0u;
    }
    assert(t->length >= 6u);
    const char * h = t->text + 2;
    while (*h != '+' && *h != '-')
        h++;
    int neg = (*h == '-');
    h += 3;
    uint64_t v = sharemind_assembler_read_hex(h, t->length - (size_t) (h - t->text));

    if (neg) {
        assert(v <= ((uint64_t) (INT64_MIN + 1)) + 1u);
        return (int64_t) -v;
    } else {
        assert(v <= (uint64_t) INT64_MAX);
        return (int64_t) v;
    }
}

SharemindAssemblerTokens * SharemindAssemblerTokens_new(void) {
    SharemindAssemblerTokens * ts = (SharemindAssemblerTokens *) malloc(sizeof(SharemindAssemblerTokens));
    if (unlikely(!ts))
        return SHAREMIND_NULL;
    ts->numTokens = 0u;
    ts->array = SHAREMIND_NULL;
    return ts;
}

void SharemindAssemblerTokens_free(SharemindAssemblerTokens * ts) {
    assert(ts);

    free(ts->array);
    free(ts);
}

SharemindAssemblerToken * SharemindAssemblerTokens_append(
        SharemindAssemblerTokens * ts,
        SharemindAssemblerTokenType type,
        const char * start,
        size_t start_line,
        size_t start_column)
{
    assert(ts);
    SharemindAssemblerToken * nts = (SharemindAssemblerToken *) realloc(ts->array, sizeof(SharemindAssemblerToken) * (ts->numTokens + 1));
    if (unlikely(!nts))
        return SHAREMIND_NULL;
    ts->array = nts;

    SharemindAssemblerToken * nt = &nts[ts->numTokens];
    ts->numTokens++;
    nt->type = type;
    nt->text = start;
    nt->start_line = start_line;
    nt->start_column = start_column;
    return nt;
}

void SharemindAssemblerTokens_pop_back_newlines(SharemindAssemblerTokens * ts) {
    assert(ts);

    if (ts->array[ts->numTokens - 1].type != SHAREMIND_ASSEMBLER_TOKEN_NEWLINE)
        return;

    ts->numTokens--;
    SharemindAssemblerToken * nts = (SharemindAssemblerToken *) realloc(ts->array, sizeof(SharemindAssemblerToken) * ts->numTokens);
    if (likely(nts))
        ts->array = nts;
}
