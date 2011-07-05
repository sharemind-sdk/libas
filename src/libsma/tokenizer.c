#include "tokenizer.h"

#include <assert.h>
#include <stdlib.h>
#include "../likely.h"
#include "tokens.h"


#define TOKENIZE_INC_CHECK_EOF(eof) \
    if (*c == '\n') { \
        sl++; \
        sc = 1; \
    } else \
        sc++; \
    if (++c == e) { \
        goto eof; \
    } else (void)0

#define NEWTOKEN(d,type,text,sl,sc) \
    if (1) { \
        d = SMA_tokens_append(ts, (type), (text), (sl), (sc)); \
        if (!d) \
            goto tokenize_error_oom; \
    }

struct SMA_Tokens * SMA_tokenize(const char * program, size_t length,
                                 size_t * errorSl, size_t *errorSc)
{
    assert(program);
    const char * c = program;
    const char * t;
    const char * const e = program + length;

    size_t sl = 1u, sc = 1u;

    struct SMA_Tokens * ts = SMA_tokens_new();
    if (unlikely(!ts))
        return NULL;
    ts->numTokens = 0u;
    struct SMA_Token * lastToken = NULL;

    size_t hexmin = 0u;

    if (unlikely(c == e))
        goto tokenize_ok;

    if (unlikely(*c == '\xff'))
        goto tokenize_bom1;
    if (unlikely(*c == '\xfe'))
        goto tokenize_bom2;
    goto tokenize_begin2;

tokenize_bom1:

    TOKENIZE_INC_CHECK_EOF(tokenize_error);
    if (likely(*c == '\xfe'))
        goto tokenize_error;
    goto tokenize_bom_end;

tokenize_bom2:

    TOKENIZE_INC_CHECK_EOF(tokenize_error);
    if (likely(*c == '\xff'))
        goto tokenize_error;

tokenize_bom_end:

    TOKENIZE_INC_CHECK_EOF(tokenize_ok);

tokenize_begin2:

    switch (*c) {
        case '\n':
            if (lastToken && lastToken->type != SMA_TOKEN_NEWLINE) {
                NEWTOKEN(lastToken, SMA_TOKEN_NEWLINE, c, sl, sc);
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
    }
    goto tokenize_error;

tokenize_comment:

    while (likely(*c != '\n')) {
        TOKENIZE_INC_CHECK_EOF(tokenize_ok);
    }
    goto tokenize_begin2;

tokenize_directive:

    switch (*c) {
        case 'a' ... 'z': case 'A' ... 'Z': case '_':
            break;
        default:
            goto tokenize_error;
    }
    NEWTOKEN(lastToken, SMA_TOKEN_DIRECTIVE, c - 1, sl, sc);
    lastToken->length = 2u;
    goto tokenize_keyword2;

tokenize_hex:

    switch (*c) {
        case '0' ... '9': case 'a' ... 'f': case 'A' ... 'F':
            break;
        default:
            goto tokenize_error;
    }
    NEWTOKEN(lastToken, SMA_TOKEN_HEX, c - 2 - hexmin, sl, sc);
    lastToken->length = 3u + hexmin;
    hexmin = 0u;

tokenize_hex2:

    TOKENIZE_INC_CHECK_EOF(tokenize_ok);
    switch (*c) {
        case '0' ... '9': case 'a' ... 'f': case 'A' ... 'F':
            if (lastToken->text[0] == '-') {
                if (lastToken->length > 18u)
                    goto tokenize_error;
                if (lastToken->length == 18u)
                    switch (lastToken->text[3]) {
                        case '8': case '9': case 'a' ... 'f': case 'A' ... 'F':
                            goto tokenize_error;
                        default:
                            break;
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
    do {
        TOKENIZE_INC_CHECK_EOF(tokenize_error);
        if (unlikely(*c == '\\')) {
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            continue;
        }
    } while (likely(*c != '"'));
    NEWTOKEN(lastToken, SMA_TOKEN_STRING, t, sl, sc);
    lastToken->length = c - t + 1u;
    TOKENIZE_INC_CHECK_EOF(tokenize_ok);
    goto tokenize_begin2;


tokenize_label:

    switch (*c) {
        case 'a' ... 'z': case 'A' ... 'Z': case '_':
            break;
        default:
            goto tokenize_error;
    }
    NEWTOKEN(lastToken, SMA_TOKEN_LABEL, c - 1, sl, sc);
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
    lastToken->type = SMA_TOKEN_LABEL_O;
    goto tokenize_hex2;

tokenize_keyword:

    NEWTOKEN(lastToken, SMA_TOKEN_KEYWORD, c, sl, sc);
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
    SMA_tokens_pop_back_newlines(ts);
    return ts;

tokenize_error:

    if (errorSl)
        *errorSl = sl;
    if (errorSc)
        *errorSc = sc;

tokenize_error_oom:

    SMA_tokens_free(ts);

    return NULL;
}
