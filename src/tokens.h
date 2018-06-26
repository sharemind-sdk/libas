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

#ifndef SHAREMIND_LIBAS_TOKENS_H
#define SHAREMIND_LIBAS_TOKENS_H

#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>


namespace sharemind {
namespace Assembler {

class Token {

    friend std::ostream & operator<<(std::ostream & os, Token const & token);

public: /* Types: */

    enum class Type {
        NEWLINE,
        DIRECTIVE,
        HEX,
        UHEX,
        STRING,
        LABEL_O,
        LABEL,
        KEYWORD
    };

public: /* Methods: */

    Token(Type type,
          char const * text,
          std::size_t length,
          std::size_t startLine,
          std::size_t startColumn) noexcept;

    Token(Token &&) noexcept = default;
    Token(Token const &) = default;

    Token & operator=(Token &&) noexcept = default;
    Token & operator=(Token const &) = default;

    Type type() const noexcept { return m_type; }

    std::int64_t hexValue() const noexcept {
        assert(m_type == Type::HEX);
        return m_parsedNumeric.hex;
    }

    std::uint64_t uhexValue() const noexcept {
        assert(m_type == Type::UHEX);
        return m_parsedNumeric.uhex;
    }

    std::string const & directiveValue() const noexcept {
        assert(m_type == Type::DIRECTIVE);
        return m_parsedString;
    }

    std::string const & stringValue() const noexcept {
        assert(m_type == Type::STRING);
        return m_parsedString;
    }

    std::string const & labelValue() const noexcept {
        assert((m_type == Type::LABEL) || (m_type == Type::LABEL_O));
        return m_parsedString;
    }

    std::int64_t labelOffset() const noexcept {
        assert((m_type == Type::LABEL) || (m_type == Type::LABEL_O));
        return m_parsedNumeric.hex;
    }

    std::string const & keywordValue() const noexcept {
        assert(m_type == Type::KEYWORD);
        return m_parsedString;
    }

private: /* Fields: */

    Type m_type;
    char const * m_text;
    std::size_t m_length;
    std::size_t m_startLine;
    std::size_t m_startColumn;
    std::string m_parsedString;
    union ParsedNumeric {
        ParsedNumeric() noexcept : hex(0) {}
        std::int64_t hex;
        std::uint64_t uhex;
    } m_parsedNumeric;

};

std::ostream & operator<<(std::ostream & os, Token::Type const type);
std::ostream & operator<<(std::ostream & os, Token const & token);

class TokensVector: public std::vector<Token> {

public: /* Methods: */

    using std::vector<Token>::vector;
    using std::vector<Token>::operator=;

    void popBackNewlines() noexcept;

};

} /* namespace Assembler { */
} /* namespace sharemind { */

#endif /* SHAREMIND_LIBAS_TOKENS_H */
