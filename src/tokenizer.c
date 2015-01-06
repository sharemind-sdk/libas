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

#include "tokenizer.h"

#include <assert.h>
#include <sharemind/likely.h>
#include <stdlib.h>


#define TOKENIZE_INC_CHECK_EOF(eof) \
    do { \
        if (*c == '\n') { \
            sl++; \
            sc = 1; \
        } else \
            sc++; \
        if (++c == e) \
            goto eof; \
    } while (0)

#define NEWTOKEN(d,type,text,sl,sc) \
    do { \
        d = SharemindAssemblerTokens_append(ts, (type), (text), (sl), (sc)); \
        if (!d) \
            goto tokenize_error_oom; \
    } while (0)

SharemindAssemblerTokens * sharemind_assembler_tokenize(
        const char * program,
        size_t length,
        size_t * errorSl,
        size_t *errorSc)
{
    assert(program);

    const char * c = program;
    const char * t;
    const char * const e = c + length;

    size_t sl = 1u, sc = 1u;

    SharemindAssemblerTokens * ts = SharemindAssemblerTokens_new();
    if (unlikely(!ts))
        return NULL;
    ts->numTokens = 0u;
    SharemindAssemblerToken * lastToken = NULL;

    size_t hexmin = 0u;
    size_t hexstart = 0u;

    if (unlikely(c == e))
        goto tokenize_ok;

    /* Lex optional UTF-8 byte-order mark 0xefbbbf */
    if (unlikely(*c == '\xef')) {
        TOKENIZE_INC_CHECK_EOF(tokenize_error);
        if (unlikely(*c != '\xbb'))
            goto tokenize_error;
        TOKENIZE_INC_CHECK_EOF(tokenize_error);
        if (unlikely(*c != '\xbf'))
            goto tokenize_error;
        TOKENIZE_INC_CHECK_EOF(tokenize_ok);
    }

tokenize_begin2:

    switch (*c) {
        case '\n':
            if (lastToken && lastToken->type != SHAREMIND_ASSEMBLER_TOKEN_NEWLINE) {
                NEWTOKEN(lastToken, SHAREMIND_ASSEMBLER_TOKEN_NEWLINE, c, sl, sc);
                lastToken->length = 1u;
            }
        case ' ': case '\t': case '\r': case '\v': case '\f':
            TOKENIZE_INC_CHECK_EOF(tokenize_ok);
            goto tokenize_begin2;
        case '#':
            TOKENIZE_INC_CHECK_EOF(tokenize_ok);
            goto tokenize_comment;
        case '.':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            goto tokenize_directive;
        case '+':
        case '-':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            if (*c != '0')
                goto tokenize_error;
            hexmin = 1u;
        case '0':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            if (unlikely(*c != 'x'))
                goto tokenize_error;
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            goto tokenize_hex;
        case '"':
            goto tokenize_string;
        case ':':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            goto tokenize_label;
        case 'a' ... 'z': case 'A' ... 'Z': case '_':
            goto tokenize_keyword;
        default:
            goto tokenize_error;
    }

tokenize_comment:

    while (likely(*c != '\n'))
        TOKENIZE_INC_CHECK_EOF(tokenize_ok);

    goto tokenize_begin2;

tokenize_directive:

    switch (*c) {
        case 'a' ... 'z': case 'A' ... 'Z': case '_':
            break;
        default:
            goto tokenize_error;
    }
    NEWTOKEN(lastToken, SHAREMIND_ASSEMBLER_TOKEN_DIRECTIVE, c - 1, sl, sc);
    lastToken->length = 2u;
    goto tokenize_keyword2;

tokenize_hex:

    switch (*c) {
        case '0' ... '9': case 'a' ... 'f': case 'A' ... 'F':
            break;
        default:
            goto tokenize_error;
    }
    NEWTOKEN(lastToken, hexmin ? SHAREMIND_ASSEMBLER_TOKEN_HEX : SHAREMIND_ASSEMBLER_TOKEN_UHEX, c - 2 - hexmin, sl, sc);
    lastToken->length = 3u + hexmin;
    hexmin = 0u;
    hexstart = 0u;

tokenize_hex2:

    TOKENIZE_INC_CHECK_EOF(tokenize_ok);
    switch (*c) {
        case '0' ... '9': case 'a' ... 'f': case 'A' ... 'F':
            if (lastToken->text[hexstart] == '-' || lastToken->text[hexstart] == '+') {
                if (lastToken->length > 18u)
                    goto tokenize_error;
                if (lastToken->length == 18u) {
                    if (lastToken->text[hexstart] == '-' && sharemind_assembler_read_hex(lastToken->text + 3, 18u) > ((uint64_t) -(INT64_MIN + 1)) + 1u)
                        goto tokenize_error;
                    if (lastToken->text[hexstart] == '+' && sharemind_assembler_read_hex(lastToken->text + 3, 18u) > (uint64_t) INT64_MAX)
                        goto tokenize_error;
                }
            } else {
                if (lastToken->length >= 18u)
                    goto tokenize_error;
            }
            break;
        case ' ': case '\t': case '\r': case '\v': case '\f':
            TOKENIZE_INC_CHECK_EOF(tokenize_ok);
        case '\n':
            goto tokenize_begin2;
        default:
            goto tokenize_error;
    }
    lastToken->length++;
    goto tokenize_hex2;

tokenize_string:

    t = c;
    for (;;) {
        TOKENIZE_INC_CHECK_EOF(tokenize_error);
        if (unlikely(*c == '\\')) {
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            continue;
        }

        if (unlikely(*c == '"'))
            break;
    }
    NEWTOKEN(lastToken, SHAREMIND_ASSEMBLER_TOKEN_STRING, t, sl, sc);
    assert(t < c);
    lastToken->length = (size_t) (c - t + 1u);
    TOKENIZE_INC_CHECK_EOF(tokenize_ok);
    goto tokenize_begin2;


tokenize_label:

    switch (*c) {
        case 'a' ... 'z': case 'A' ... 'Z': case '_':
            break;
        default:
            goto tokenize_error;
    }
    NEWTOKEN(lastToken, SHAREMIND_ASSEMBLER_TOKEN_LABEL, c - 1, sl, sc);
    lastToken->length = 2u;

tokenize_label2:

    TOKENIZE_INC_CHECK_EOF(tokenize_ok);
    switch (*c) {
        case '0' ... '9': case 'a' ... 'z': case 'A' ... 'Z': case '_':
            lastToken->length++;
            break;
        case ' ': case '\t': case '\r': case '\v': case '\f':
            TOKENIZE_INC_CHECK_EOF(tokenize_ok);
        case '\n':
            goto tokenize_begin2;
        case '.':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            switch (*c) {
                case 'a' ... 'z': case 'A' ... 'Z': case '_':
                    break;
                default:
                    goto tokenize_error;
            }
            lastToken->length += 2u;
            break;
        case '+': case '-':
            hexstart = lastToken->length - 1u;
            goto tokenize_label3;
        default:
            goto tokenize_error;
    }
    goto tokenize_label2;

tokenize_label3:

    TOKENIZE_INC_CHECK_EOF(tokenize_error);
    if (unlikely(*c != '0'))
        goto tokenize_error;
    TOKENIZE_INC_CHECK_EOF(tokenize_error);
    if (unlikely(*c != 'x'))
        goto tokenize_error;
    TOKENIZE_INC_CHECK_EOF(tokenize_error);
    switch (*c) {
        case '0' ... '9': case 'a' ... 'f': case 'A' ... 'F':
            break;
        default:
            goto tokenize_error;
    }
    lastToken->length += 4u;
    lastToken->type = SHAREMIND_ASSEMBLER_TOKEN_LABEL_O;
    goto tokenize_hex2;

tokenize_keyword:

    NEWTOKEN(lastToken, SHAREMIND_ASSEMBLER_TOKEN_KEYWORD, c, sl, sc);
    lastToken->length = 1u;
    goto tokenize_keyword2;

tokenize_keyword2:

    TOKENIZE_INC_CHECK_EOF(tokenize_ok);
    switch (*c) {
        case '0' ... '9': case 'a' ... 'z': case 'A' ... 'Z': case '_':
            lastToken->length++;
            break;
        case ' ': case '\t': case '\r': case '\v': case '\f':
            TOKENIZE_INC_CHECK_EOF(tokenize_ok);
        case '\n':
            goto tokenize_begin2;
        case '.':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            switch (*c) {
                case 'a' ... 'z': case 'A' ... 'Z': case '_':
                    break;
                default:
                    goto tokenize_error;
            }
            lastToken->length += 2u;
            break;
        default:
            goto tokenize_error;
    }
    goto tokenize_keyword2;

tokenize_ok:
    SharemindAssemblerTokens_pop_back_newlines(ts);
    return ts;

tokenize_error:

    if (errorSl)
        *errorSl = sl;
    if (errorSc)
        *errorSc = sc;

tokenize_error_oom:

    SharemindAssemblerTokens_free(ts);

    return NULL;
}
