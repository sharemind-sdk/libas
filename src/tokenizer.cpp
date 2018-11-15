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
#include <sharemind/Concat.h>
#include <sharemind/likely.h>
#include "Exception.h"


namespace sharemind {
namespace Assembler {

SHAREMIND_DEFINE_EXCEPTION_CONST_STDSTRING_NOINLINE(Exception,,
                                                    TokenizerException);

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

#define SEPARATOR_WHITESPACE ' ': case '\t': case '\r': case '\v': case '\f'

#define SIMPLE_ERROR_OUT(...) throw TokenizerException(__VA_ARGS__)
#define ERROR_OUT(...) SIMPLE_ERROR_OUT(concat(__VA_ARGS__))

#define TOKENIZE_INC_CHECK_EOF(...) \
    do { \
        if (*c == '\n') { \
            sl++; \
            sc = 1; \
        } else \
            sc++; \
        if (++c == e) \
            { __VA_ARGS__ } \
    } while (0)
#define TOKENIZE_INC_CHECK_EOF_OK TOKENIZE_INC_CHECK_EOF(goto tokenize_ok;)

std::string asciiCharToPrintable(char const c) {
    switch (c) {
        case ASCII_LETTER:
        case DECIMAL_DIGIT:
        case ' ': case '!': case '"': case '#': case '$': case '%': case '&':
        case '\'': case '(': case ')': case '*': case '+': case ',': case '-':
        case '.': case '/': case ':': case ';': case '<': case '=': case '>':
        case '?': case '@': case '[': case '\\': case ']': case '^': case '_':
        case '`': case '{': case '|': case '}': case '~':
            return std::string{c};
        case '\a': return "\a";
        case '\b': return "\b";
        case '\f': return "\f";
        case '\n': return "\n";
        case '\r': return "\r";
        case '\t': return "\t";
        case '\v': return "\v";
        default:
            return concat("\\x", std::hex, static_cast<unsigned>(c));
    }
}

#define ERROR_OUT_UNEXPECTED(expected, found, what) \
    ERROR_OUT("Expected " expected " while parsing " what ", but '", \
               asciiCharToPrintable((found)), "' encountered!")
#define TOKENIZE_INC_CHECK_EOF_UNEXPECTED(...) \
    TOKENIZE_INC_CHECK_EOF(SIMPLE_ERROR_OUT( \
            "Unexpected end-of-file while parsing " __VA_ARGS__ "!" );)

#define NEWTOKEN(d,type,text,len,sl,sc) \
    do { \
        ts.emplace_back((type), (text), (len), (sl), (sc)); \
        d = &ts.back(); \
    } while (0)

TokensVector tokenize(char const * program, std::size_t length) {
    assert(program);

    char const * c = program;
    char const * t;
    char const * const e = c + length;

    std::size_t sl = 1u;
    std::size_t sc = 1u;

    TokensVector ts;
    Token * lastToken = nullptr;

    char hexstart;

    if (unlikely(c == e))
        goto tokenize_ok;

    /* Lex optional UTF-8 byte-order mark 0xefbbbf */
    if (unlikely(*c == '\xef')) {
        TOKENIZE_INC_CHECK_EOF_UNEXPECTED("UTF-8 byte-order-mark");
        if (unlikely(*c != '\xbb'))
            SIMPLE_ERROR_OUT("Invalid UTF-8 byte-order-mark!");
        TOKENIZE_INC_CHECK_EOF_UNEXPECTED("UTF-8 byte-order-mark");
        if (unlikely(*c != '\xbf'))
            SIMPLE_ERROR_OUT("Invalid UTF-8 byte-order-mark!");
        TOKENIZE_INC_CHECK_EOF_OK;
    }
#define HANDLE_SEPARATOR_WHITESPACE \
    do { \
        TOKENIZE_INC_CHECK_EOF_OK; \
        goto tokenize_begin; \
    } while(false)
#define HANDLE_COMMENT \
    do { \
        TOKENIZE_INC_CHECK_EOF_OK; \
        while (likely(*c != '\n')) \
            TOKENIZE_INC_CHECK_EOF_OK; \
        goto tokenize_begin; \
    } while(false)
#define TOKEN_END_CASES(...) \
    case SEPARATOR_WHITESPACE: \
        __VA_ARGS__ \
        HANDLE_SEPARATOR_WHITESPACE; \
    case '\n': \
        __VA_ARGS__ \
        NEWTOKEN(lastToken, Token::Type::NEWLINE, c, 1u, sl, sc); \
        TOKENIZE_INC_CHECK_EOF_OK; \
        goto tokenize_begin; \
    case '#': \
        __VA_ARGS__ \
        HANDLE_COMMENT

#define CREATE_START_COUNTED_TOKEN(type, what) \
    do { \
        NEWTOKEN(lastToken, \
                 Token::Type::type, \
                 what ## Start, \
                 static_cast<std::size_t>(c - what ## Start), \
                 what ## StartLine, \
                 what ## StartColumn); \
    } while (false)
#define TOKENIZE_KEYWORD_OR_DIRECTIVE(start, startCol, type, what) \
    do { \
        auto const what ## Start = start; \
        auto const what ## StartLine = sl; \
        auto const what ## StartColumn = startCol; \
        for (;;) { \
            TOKENIZE_INC_CHECK_EOF(CREATE_START_COUNTED_TOKEN(type, what););\
            switch (*c) { \
                case ID_TAIL: \
                    break; \
                case '.': \
                    TOKENIZE_INC_CHECK_EOF_UNEXPECTED(#what); \
                    switch (*c) { \
                        case ID_HEAD: \
                            break; \
                        default: \
                            ERROR_OUT_UNEXPECTED("letter or underscore", \
                                                 *c, #what); \
                    } \
                    break; \
                TOKEN_END_CASES(CREATE_START_COUNTED_TOKEN(type, what);); \
                default: \
                    ERROR_OUT_UNEXPECTED("letter, underscore, '.', " \
                                         "whitespace, comment or end-of-file", \
                                         *c, #what); \
            } \
        } \
    } while (false)
#define TOKENIZE_HEX_FROM_0_CHECK_N_CREATE(type, id, what) \
    do { \
        /* Check signed range (min -8000000000000000, max 7fffffffffffffff): */\
        if (hexstart == '+') { \
            switch (*(c - 16u)) { \
            case '8': case '9': \
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': \
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': \
                SIMPLE_ERROR_OUT("64-bit signed hexadecimal too big while " \
                                 "parsing " what "!"); \
            default: break; \
            } \
        } else if (hexstart == '-') { \
            auto c2 = c - 16u; /* Initially pointer to first digit. */ \
            switch (*c2) { \
            case '8': \
                while (++c2 != c) \
                    if (*c2 != '0') \
                        SIMPLE_ERROR_OUT("64-bit signed hexadecimal too small "\
                                         "while parsing " what "!"); \
                break; \
            case '9': \
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': \
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': \
                SIMPLE_ERROR_OUT("64-bit signed hexadecimal too big while " \
                                 "parsing " what "!"); \
            default: break; \
            } \
        } \
        CREATE_START_COUNTED_TOKEN(type, id); \
    } while (false)
#define TOKENIZE_HEX_FROM_0(start, startCol, type, id, what) \
    do { \
        assert(*c == '0'); \
        assert((hexstart == '0') || (hexstart == '+') || (hexstart == '-')); \
        auto const id ## Start = (start); \
        auto const id ## StartLine = sl; \
        auto const id ## StartColumn = (startCol); \
        TOKENIZE_INC_CHECK_EOF_UNEXPECTED(what); \
        if (unlikely(*c != 'x')) \
            ERROR_OUT_UNEXPECTED("'x'", *c, what); \
        TOKENIZE_INC_CHECK_EOF_UNEXPECTED(what); \
        switch (*c) { /* First digit: */ \
            case HEXADECIMAL_DIGIT: break; \
            default: ERROR_OUT_UNEXPECTED("hexadecimal digit", *c, what); \
        } \
        unsigned digitsLeft = 15u; \
        do { \
            TOKENIZE_INC_CHECK_EOF(CREATE_START_COUNTED_TOKEN(type, id); \
                                   goto tokenize_ok;); \
            switch (*c) { \
                case HEXADECIMAL_DIGIT: \
                    break; \
                TOKEN_END_CASES(CREATE_START_COUNTED_TOKEN(type, id););\
                default: \
                    ERROR_OUT_UNEXPECTED("hexadecimal digit, whitespace, " \
                                         "comment or end-of-file", \
                                         *c, what); \
            } \
        } while (--digitsLeft); \
        TOKENIZE_INC_CHECK_EOF( \
                TOKENIZE_HEX_FROM_0_CHECK_N_CREATE(type, id, what); \
                goto tokenize_ok;); \
        switch (*c) { \
        case HEXADECIMAL_DIGIT: \
            SIMPLE_ERROR_OUT("64-bit hexadecimal has too many digits while " \
                             "parsing " what "!"); \
        TOKEN_END_CASES(TOKENIZE_HEX_FROM_0_CHECK_N_CREATE(type, id, what);); \
        default: \
            ERROR_OUT_UNEXPECTED("whitespace, comment or end-of-file", \
                                 *c, what); \
        } \
    } while (false)

tokenize_begin:

    switch (*c) {
        case '\n':
            if (lastToken && lastToken->type() != Token::Type::NEWLINE)
                NEWTOKEN(lastToken, Token::Type::NEWLINE, c, 1u, sl, sc);
            /* FALLTHROUGH */
        case SEPARATOR_WHITESPACE:
            HANDLE_SEPARATOR_WHITESPACE;
        case '#':
            HANDLE_COMMENT;
        case '.':
            TOKENIZE_INC_CHECK_EOF_UNEXPECTED("directive");
            switch (*c) {
                case ID_HEAD:
                    break;
                default:
                    ERROR_OUT_UNEXPECTED("letter or underscore",
                                         *c,
                                         "directive");
            }
            TOKENIZE_KEYWORD_OR_DIRECTIVE(c - 1u,
                                          sc - 1u,
                                          DIRECTIVE,
                                          directive);
        case '+':
        case '-':
            hexstart = *c;
            TOKENIZE_INC_CHECK_EOF_UNEXPECTED("signed hexadecimal");
            if (*c != '0')
                ERROR_OUT_UNEXPECTED("'0'", *c, "signed hexadecimal");
            TOKENIZE_HEX_FROM_0(c - 1u,
                                sc - 1u,
                                HEX,
                                signedHexadecimal,
                                "signed hexadecimal");
        case '0':
            hexstart = *c;
            TOKENIZE_HEX_FROM_0(c,
                                sc,
                                UHEX,
                                unsignedHexadecimal,
                                "unsigned hexadecimal");
        case '"':
            t = c;
            for (;;) {
                TOKENIZE_INC_CHECK_EOF_UNEXPECTED("string");
                if (unlikely(*c == '\\')) {
                    TOKENIZE_INC_CHECK_EOF_UNEXPECTED("string");
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
            TOKENIZE_INC_CHECK_EOF_OK;
            goto tokenize_begin;
        case ':': {
            TOKENIZE_INC_CHECK_EOF_UNEXPECTED("label");
            switch (*c) {
                case ID_HEAD:
                    break;
                default:
                    ERROR_OUT_UNEXPECTED("letter or underscore", *c, "label");
            }
            auto const labelStart = c - 1u;
            auto const labelStartLine = sl;
            auto const labelStartColumn = sc - 1u;
            for (;;) {
                TOKENIZE_INC_CHECK_EOF();
                switch (*c) {
                    case ID_TAIL:
                        break;
                    case '+': case '-':
                        hexstart = *c;
                        TOKENIZE_INC_CHECK_EOF_UNEXPECTED("label offset");
                        if (unlikely(*c != '0'))
                            ERROR_OUT_UNEXPECTED("'0'", *c, "label offset");
                        TOKENIZE_HEX_FROM_0(labelStart,
                                            labelStartColumn,
                                            LABEL_O,
                                            labelWithOffset,
                                            "label with offset");
                    TOKEN_END_CASES(CREATE_START_COUNTED_TOKEN(LABEL, label););
                    default:
                        ERROR_OUT("Unexpected '", asciiCharToPrintable(*c),
                                  "' found while parsing label!");
                }
            }
        }
        case ID_HEAD:
            TOKENIZE_KEYWORD_OR_DIRECTIVE(c, sc, KEYWORD, keyword);
        default:
            ERROR_OUT("Unexpected '", asciiCharToPrintable(*c), "' found!");
    }

tokenize_ok:
    ts.popBackNewlines();
    return ts;
}

} // namespace Assembler {
} // namespace sharemind {
