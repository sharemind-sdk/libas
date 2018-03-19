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

#ifndef SHAREMIND_LIBAS_ASSEMBLE_H
#define SHAREMIND_LIBAS_ASSEMBLE_H

#include <sharemind/AssertReturn.h>
#include <sharemind/ExceptionMacros.h>
#include <sharemind/preprocessor.h>
#include "Exception.h"
#include "linker.h"
#include "tokens.h"


namespace sharemind {
namespace Assembler {

class AssembleException: public Exception {

private: /* Types: */

    struct Data {

    /* Methods: */

        template <typename ... Args>
        Data(TokensVector::const_iterator tokenIterator,
             Args && ... args)
            : m_tokenIterator(tokenIterator)
            , m_message(std::forward<Args>(args)...)
        {}

    /* Fields: */

        TokensVector::const_iterator m_tokenIterator;
        std::string m_message;

    };

public: /* Methods: */

    template <typename ... Args>
    AssembleException(Args && ... args)
        : m_data(std::make_shared<Data>(std::forward<Args>(args)...))
    {}

    char const * what() const noexcept final override
    { return assertReturn(m_data)->m_message.c_str(); }

    TokensVector::const_iterator const & tokenIterator() noexcept
    { return assertReturn(m_data)->m_tokenIterator; }

private: /* Fields: */

    std::shared_ptr<Data> m_data;

};

LinkingUnitsVector assemble(TokensVector const & ts);

} /* namespace Assembler { */
} /* namespace sharemind { */

#endif /* SHAREMIND_LIBAS_ASSEMBLE_H */
