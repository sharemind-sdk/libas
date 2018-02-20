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
#include <limits>
#include <sharemind/abort.h>
#include <sharemind/IntegralComparisons.h>
#include <sharemind/SignedToUnsigned.h>
#include <sharemind/likely.h>
#include <sharemind/null.h>


namespace sharemind {

namespace {

constexpr auto const int64Min = std::numeric_limits<std::int64_t>::min();
constexpr auto const int64Max = std::numeric_limits<std::int64_t>::max();
constexpr auto const absInt64Min = signedToUnsigned(-(int64Min + 1)) + 1u;

} // anonymous namespace

std::uint64_t assembler_read_hex(char const * c, std::size_t l) {
    assert(c);

    auto * const e = c + l;
    std::uint64_t v = 0u;
    do {
        std::uint64_t digit;
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
                std::abort();
        }
        v = (v * 16u) + digit;
    } while (++c < e);
    return v;
}

std::int64_t AssemblerToken::hexValue() const {
    assert(type == Type::HEX);
    assert(length >= 4u);
    assert(length <= 19u);
    assert(text[0u] == '-' || text[0u] == '+');
    assert(text[1u] == '0');
    assert(text[2u] == 'x');

    auto v = assembler_read_hex(text + 3u, length - 3u);

    if (text[0] == '-') {
        assert(v <= absInt64Min);
        return -static_cast<std::int64_t>(v);
    } else {
        assert(text[0] == '+');
        assert(integralLessEqual(v, int64Max));
        return static_cast<std::int64_t>(v);
    }
}

std::uint64_t AssemblerToken::uhexValue() const {
    assert(type == Type::UHEX);
    assert(length >= 3u);
    assert(length <= 18u);
    assert(text[0u] == '0');
    assert(text[1u] == 'x');

    return assembler_read_hex(text + 2u, length - 2u);
}

std::size_t AssemblerToken::stringLength() const {
    assert(type == Type::STRING);
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

std::string AssemblerToken::stringValue() const{
    assert(type == Type::STRING);
    auto const l = stringLength();
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

std::string AssemblerToken::labelToString() const {
    if (type == Type::LABEL) {
        assert(length >= 2u);
        return {text + 1u, length - 1u};
    } else {
        assert(type == Type::LABEL_O);
        assert(length >= 6u);
        std::size_t l;
        for (l = 2; text[l] != '+' && text[l] != '-'; l++) /* Do nothing */;
        assert(text[l + 1] == '0');
        assert(text[l + 2] == 'x');
        return {text + 1u, l - 1u};
    }
}

std::int64_t AssemblerToken::labelOffset() const {
    if (type == Type::LABEL) {
        assert(text[0u] == ':');
        assert(length >= 2u);
        return 0u;
    }
    assert(type == Type::LABEL_O);
    assert(text[0u] == ':');
    assert(length >= 6u);
    char const * h = text + 2;
    while ((*h != '+') && (*h != '-'))
        ++h;
    bool const neg = (*h == '-');
    h += 3;
    auto v = assembler_read_hex(h, length - static_cast<std::size_t>(h - text));

    if (neg) {
        assert(v <= absInt64Min);
        return -static_cast<std::int64_t>(v);
    } else {
        assert(integralLessEqual(v, int64Max));
        return static_cast<std::int64_t>(v);
    }
}

std::ostream & operator<<(std::ostream & os, AssemblerToken::Type const type) {
    #define SHAREMIND_LIBAS_TOKENS_T(v) \
            case AssemblerToken::Type::v: os << #v; break
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

std::ostream & operator<<(std::ostream & os, AssemblerToken const & token) {
    if (token.type == AssemblerToken::Type::NEWLINE)
        return os << token.type;
    os << token.type << '(';
    /// \todo Optimize this:
    for (std::size_t i = 0u; i < token.length; ++i)
        os << token.text[i];
    return os << ")@" << token.start_line << ':' << token.start_column;
}

void AssemblerTokens::popBackNewlines() noexcept {
    while (!empty()) {
        if (back().type != AssemblerToken::Type::NEWLINE)
            return;
        pop_back();
    }
}

} // namespace sharemind {
