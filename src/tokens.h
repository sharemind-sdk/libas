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

#ifndef SHAREMIND_LIBAS_TOKENS_H
#define SHAREMIND_LIBAS_TOKENS_H

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>


namespace sharemind {

std::uint64_t assembler_read_hex(const char * c, size_t l)
        __attribute__ ((nonnull(1)));

struct AssemblerToken {

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

/* Methods: */

    AssemblerToken(Type type_,
                   char const * text_,
                   std::size_t length_,
                   std::size_t startLine_,
                   std::size_t startColumn_) noexcept
        : type(type_)
        , text(text_)
        , length(length_)
        , start_line(startLine_)
        , start_column(startColumn_)
    {}

    AssemblerToken(AssemblerToken &&) noexcept = default;
    AssemblerToken(AssemblerToken const &) noexcept = default;

    AssemblerToken & operator=(AssemblerToken &&) noexcept = default;
    AssemblerToken & operator=(AssemblerToken const &) noexcept = default;

    std::int64_t hexValue() const;
    std::uint64_t uhexValue() const;

    std::size_t stringLength() const;
    std::string stringValue() const;

    std::string labelToString() const;
    std::int64_t labelOffset() const;

/* Fields: */

    Type type;
    char const * text;
    std::size_t length;
    std::size_t start_line;
    std::size_t start_column;

};

std::ostream & operator<<(std::ostream & os, AssemblerToken::Type const type);
std::ostream & operator<<(std::ostream & os, AssemblerToken const & token);

class AssemblerTokens: public std::vector<AssemblerToken> {

public: /* Methods: */

    using std::vector<AssemblerToken>::vector;
    using std::vector<AssemblerToken>::operator=;

    void popBackNewlines() noexcept;

};

} // namespace sharemind {

#endif /* SHAREMIND_LIBAS_TOKENS_H */
