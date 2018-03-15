/*
 * Copyright (C) Cybernetica
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

#include <cassert>
#include <sharemind/likely.h>
#include <sharemind/MakeUnique.h>


namespace sharemind {
namespace Assembler {

#define DECIMAL_DIGIT \
         '0': case '1': case '2': case '3': case '4': \
    case '5': case '6': case '7': case '8': case '9'

#define HEXADECIMAL_DIGIT \
    DECIMAL_DIGIT: \
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': \
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F'

#define ASCII_LOWERCASE_LETTER \
         'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': \
    case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': \
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u': \
    case 'v': case 'w': case 'x': case 'y': case 'z'

#define ASCII_UPPERCASE_LETTER \
         'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': \
    case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': \
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': \
    case 'V': case 'W': case 'X': case 'Y': case 'Z'

#define ASCII_LETTER \
    ASCII_LOWERCASE_LETTER: case ASCII_UPPERCASE_LETTER

#define ID_HEAD ASCII_LETTER: case '_'
#define ID_TAIL ASCII_LETTER: case DECIMAL_DIGIT: case '_'

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

#define NEWTOKEN(d,type,text,len,sl,sc) \
    do { \
        ts->emplace_back((type), (text), (len), (sl), (sc)); \
        d = &ts->back(); \
    } while (0)

std::unique_ptr<TokensVector> tokenize(char const * program,
                                       std::size_t length,
                                       std::size_t * errorSl,
                                       std::size_t * errorSc)
{
    assert(program);

    char const * c = program;
    char const * t;
    char const * const e = c + length;

    std::size_t sl = 1u;
    std::size_t sc = 1u;

    auto ts(makeUnique<TokensVector>());
    Token * lastToken = nullptr;

    std::size_t hexmin = 0u;
    std::size_t hexstart = 0u;

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

tokenize_begin:

    switch (*c) {
        case '\n':
            if (lastToken && lastToken->type != Token::Type::NEWLINE)
                NEWTOKEN(lastToken, Token::Type::NEWLINE, c, 1u, sl, sc);
            /* FALLTHROUGH */
        case ' ': case '\t': case '\r': case '\v': case '\f':
            TOKENIZE_INC_CHECK_EOF(tokenize_ok);
            goto tokenize_begin;
        case '#':
            TOKENIZE_INC_CHECK_EOF(tokenize_ok);
            while (likely(*c != '\n'))
                TOKENIZE_INC_CHECK_EOF(tokenize_ok);
            goto tokenize_begin;
        case '.':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            switch (*c) {
                case ID_HEAD:
                    break;
                default:
                    goto tokenize_error;
            }
            NEWTOKEN(lastToken, Token::Type::DIRECTIVE, c - 1, 2u, sl, sc);
            goto tokenize_keyword;
        case '+':
        case '-':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            if (*c != '0')
                goto tokenize_error;
            hexmin = 1u;
            /* FALLTHROUGH */
        case '0':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            if (unlikely(*c != 'x'))
                goto tokenize_error;
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            switch (*c) {
                case HEXADECIMAL_DIGIT:
                    break;
                default:
                    goto tokenize_error;
            }
            NEWTOKEN(lastToken,
                     hexmin ? Token::Type::HEX : Token::Type::UHEX,
                     c - 2 - hexmin,
                     3u + hexmin,
                     sl,
                     sc);
            hexmin = 0u;
            hexstart = 0u;
            goto tokenize_hex;
        case '"':
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
            assert(t < c);
            NEWTOKEN(lastToken,
                     Token::Type::STRING,
                     t,
                     static_cast<std::size_t>(c - t + 1u),
                     sl,
                     sc);
            TOKENIZE_INC_CHECK_EOF(tokenize_ok);
            goto tokenize_begin;
        case ':':
            TOKENIZE_INC_CHECK_EOF(tokenize_error);
            switch (*c) {
                case ID_HEAD:
                    break;
                default:
                    goto tokenize_error;
            }
            NEWTOKEN(lastToken, Token::Type::LABEL, c - 1, 2u, sl, sc);
            for (;;) {
                TOKENIZE_INC_CHECK_EOF(tokenize_ok);
                switch (*c) {
                    case ID_TAIL:
                        lastToken->length++;
                        break;
                    case ' ': case '\t': case '\r': case '\v': case '\f':
                        TOKENIZE_INC_CHECK_EOF(tokenize_ok);
                    case '\n':
                        goto tokenize_begin;
                    case '.':
                        TOKENIZE_INC_CHECK_EOF(tokenize_error);
                        switch (*c) {
                            case ID_HEAD:
                                break;
                            default:
                                goto tokenize_error;
                        }
                        lastToken->length += 2u;
                        break;
                    case '+': case '-':
                        hexstart = lastToken->length - 1u;
                        TOKENIZE_INC_CHECK_EOF(tokenize_error);
                        if (unlikely(*c != '0'))
                            goto tokenize_error;
                        TOKENIZE_INC_CHECK_EOF(tokenize_error);
                        if (unlikely(*c != 'x'))
                            goto tokenize_error;
                        TOKENIZE_INC_CHECK_EOF(tokenize_error);
                        switch (*c) {
                            case HEXADECIMAL_DIGIT:
                                break;
                            default:
                                goto tokenize_error;
                        }
                        lastToken->length += 4u;
                        lastToken->type = Token::Type::LABEL_O;
                        goto tokenize_hex;
                    default:
                        goto tokenize_error;
                }
            }
        case ID_HEAD:
            NEWTOKEN(lastToken, Token::Type::KEYWORD, c, 1u, sl, sc);
            goto tokenize_keyword;
        default:
            goto tokenize_error;
    }

tokenize_hex:

    for (;;) {
        TOKENIZE_INC_CHECK_EOF(tokenize_ok);
        switch (*c) {
            case HEXADECIMAL_DIGIT:
                if (lastToken->text[hexstart] == '-' || lastToken->text[hexstart] == '+') {
                    if (lastToken->length > 18u)
                        goto tokenize_error;
                    if (lastToken->length == 18u) {
                        if (lastToken->text[hexstart] == '-' && readHex(lastToken->text + 3, 18u) > ((std::uint64_t) -(INT64_MIN + 1)) + 1u)
                            goto tokenize_error;
                        if (lastToken->text[hexstart] == '+' && readHex(lastToken->text + 3, 18u) > (std::uint64_t) INT64_MAX)
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
                goto tokenize_begin;
            default:
                goto tokenize_error;
        }
        lastToken->length++;
    }


tokenize_keyword:

    for (;;) {
        TOKENIZE_INC_CHECK_EOF(tokenize_ok);
        switch (*c) {
            case ID_TAIL:
                lastToken->length++;
                break;
            case ' ': case '\t': case '\r': case '\v': case '\f':
                TOKENIZE_INC_CHECK_EOF(tokenize_ok);
            case '\n':
                goto tokenize_begin;
            case '.':
                TOKENIZE_INC_CHECK_EOF(tokenize_error);
                switch (*c) {
                    case ID_HEAD:
                        break;
                    default:
                        goto tokenize_error;
                }
                lastToken->length += 2u;
                break;
            default:
                goto tokenize_error;
        }
    }

tokenize_ok:
    ts->popBackNewlines();
    return ts;

tokenize_error:

    if (errorSl)
        *errorSl = sl;
    if (errorSc)
        *errorSc = sc;
    return nullptr;
}

} // namespace Assembler {
} // namespace sharemind {
