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

#include "tokens.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sharemind/abort.h>
#include <sharemind/IntegralComparisons.h>
#include <sharemind/SignedToUnsigned.h>
#include <sharemind/likely.h>


namespace sharemind {
namespace Assembler {
namespace {

constexpr auto const int64Min = std::numeric_limits<std::int64_t>::min();
constexpr auto const int64Max = std::numeric_limits<std::int64_t>::max();
constexpr auto const absInt64Min = signedToUnsigned(-(int64Min + 1)) + 1u;

std::uint64_t readHex(char const * c, std::size_t l) {
    assert(c);

    auto * const e = c + l;
    std::uint64_t v = 0u;
    do {
        std::uint64_t digit;
        switch (*c) {
            #define X(c) case #c[0u] : digit = 0x ## c ## u; break
            X(0); X(1); X(2); X(3); X(4); X(5); X(6); X(7); X(8); X(9);
            X(A); X(B); X(C); X(D); X(E); X(F);
            X(a); X(b); X(c); X(d); X(e); X(f);
            #undef X
            default:
                std::abort();
        }
        v = (v * 16u) + digit;
    } while (++c < e);
    return v;
}

std::int64_t parseHexValue(char const * const text,
                           std::size_t const length)
{
    assert(length >= 4u);
    assert(length <= 19u);
    assert(text[0u] == '-' || text[0u] == '+');
    assert(text[1u] == '0');
    assert(text[2u] == 'x');

    auto v = readHex(text + 3u, length - 3u);
    if (text[0] == '-') {
        assert(v <= absInt64Min);
        return -static_cast<std::int64_t>(v);
    } else {
        assert(text[0] == '+');
        assert(integralLessEqual(v, int64Max));
        return static_cast<std::int64_t>(v);
    }
}

inline std::uint64_t parseUhexValue(char const * const text,
                                    std::size_t const length)
{
    assert(length >= 3u);
    assert(length <= 18u);
    assert(text[0u] == '0');
    assert(text[1u] == 'x');
    return readHex(text + 2u, length - 2u);
}

inline std::size_t parseStringLength(char const * const text,
                                     std::size_t const length)
{
    assert(length >= 2u);
    std::size_t l = 0u;
    for (std::size_t i = 1u; i < length - 1; i++) {
        l++;
        if (text[i] == '\\') {
            /** \todo \xFFFF.. and \377 syntax. */
            i++;
            assert(i < length);
        }
    }
    return l;
}

inline std::string parseString(char const * const text,
                               std::size_t const length)
{
    auto const l = parseStringLength(text, length);
    std::string r;
    r.reserve(l);

    auto * ip = &text[1];
    auto * const ip_end = &text[length - 1u];

    while (ip != ip_end) {
        if (*ip != '\\') {
            r.push_back(*ip);
        } else {
            ip++;
            assert(ip != ip_end);
            switch (*ip) {
                case 'n':  r.push_back('\n'); break;
                case 'r':  r.push_back('\r'); break;
                case 't':  r.push_back('\t'); break;
                case 'v':  r.push_back('\v'); break;
                case 'b':  r.push_back('\b'); break;
                case 'f':  r.push_back('\f'); break;
                case 'a':  r.push_back('\a'); break;
                case '0':  r.push_back('\0'); break; /**< \todo proper \xFFFF.. and \377 syntax. */
                case '\'': /* *op = '\''; break; */
                case '"':  /* *op = '"';  break; */
                case '?':  /* *op = '?';  break; */
                case '\\': /* *op = '\\'; break; */
                default:   r.push_back(*ip);  break;
            }
        }
        ip++;
    }
    return r;
}

inline std::int64_t parseLabelOffset(char const * const text,
                                     std::size_t const length)
{
    assert(text[0u] == ':');
    assert(length >= 6u);
    char const * h = text + 2;
    while ((*h != '+') && (*h != '-'))
        ++h;
    bool const neg = (*h == '-');
    h += 3;
    auto v = readHex(h, length - static_cast<std::size_t>(h - text));
    if (neg) {
        assert(v <= absInt64Min);
        return -static_cast<std::int64_t>(v);
    } else {
        assert(integralLessEqual(v, int64Max));
        return static_cast<std::int64_t>(v);
    }
}

} // anonymous namespace

Token::Token(Type type,
             char const * text,
             std::size_t length,
             std::size_t startLine,
             std::size_t startColumn) noexcept
    : m_type(type)
    , m_text(text)
    , m_length(length)
    , m_startLine(startLine)
    , m_startColumn(startColumn)
    , m_parsedString(
        [type, text, length]() {
            switch (type) {
                case Type::NEWLINE: break;
                case Type::HEX: break;
                case Type::UHEX: break;
                case Type::STRING: return parseString(text, length);
                case Type::LABEL:
                case Type::DIRECTIVE:
                    assert(length > 2u);
                    return std::string(text + 1u, length - 1u);
                case Type::LABEL_O: {
                    assert(length >= 6u);
                    std::size_t l = 2u;
                    while (text[l] != '+' && text[l] != '-')
                        ++l;
                    assert(text[l + 1] == '0');
                    assert(text[l + 2] == 'x');
                    return std::string(text + 1u, l - 1u);
                }
                case Type::KEYWORD: return std::string(text, length);
            }
            return std::string();
        }())
    , m_parsedNumeric(
        [type, text, length]() {
            ParsedNumeric r;
            switch (type) {
            case Type::NEWLINE: break;
            case Type::DIRECTIVE: break;
            case Type::HEX: r.hex = parseHexValue(text, length); break;
            case Type::UHEX: r.uhex = parseUhexValue(text, length); break;
            case Type::STRING: break;
            case Type::LABEL: r.hex = 0u; break;
            case Type::LABEL_O: r.hex = parseLabelOffset(text, length); break;
            case Type::KEYWORD: break;
            }
            return r;
        }())
{}

std::ostream & operator<<(std::ostream & os, Token::Type const type) {
    #define SHAREMIND_LIBAS_TOKENS_T(v) \
            case Token::Type::v: os << #v; break
    switch (type) {
        SHAREMIND_LIBAS_TOKENS_T(NEWLINE);
        SHAREMIND_LIBAS_TOKENS_T(DIRECTIVE);
        SHAREMIND_LIBAS_TOKENS_T(HEX);
        SHAREMIND_LIBAS_TOKENS_T(UHEX);
        SHAREMIND_LIBAS_TOKENS_T(STRING);
        SHAREMIND_LIBAS_TOKENS_T(LABEL_O);
        SHAREMIND_LIBAS_TOKENS_T(LABEL);
        SHAREMIND_LIBAS_TOKENS_T(KEYWORD);
    }
    #undef SHAREMIND_LIBAS_TOKENS_T
    return os;
}

std::ostream & operator<<(std::ostream & os, Token const & token) {
    if (token.m_type == Token::Type::NEWLINE)
        return os << token.m_type;
    os << token.m_type << '(';
    /// \todo Optimize this:
    for (std::size_t i = 0u; i < token.m_length; ++i)
        os << token.m_text[i];
    return os << ")@" << token.m_startLine << ':' << token.m_startColumn;
}

void TokensVector::popBackNewlines() noexcept {
    while (!empty()) {
        if (back().type() != Token::Type::NEWLINE)
            return;
        pop_back();
    }
}

} // namespace Assembler {
} // namespace sharemind {
