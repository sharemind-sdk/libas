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

#include <sharemind/ExceptionMacros.h>
#include <sharemind/preprocessor.h>
#include "Exception.h"
#include "linker.h"
#include "tokens.h"


namespace sharemind {
namespace Assembler {

class AssembleException: public Exception {

public: /* Methods: */

    template <typename ... Args>
    AssembleException(TokensVector::const_iterator tokenIt,
                      Args && ... args)
        : Exception(std::forward<Args>(args)...)
        , m_tokenIt(tokenIt)
    {}

private: /* Fields: */

    TokensVector::const_iterator m_tokenIt;

};

void assemble(TokensVector const & ts, LinkingUnitsVector & lus);

} /* namespace Assembler { */
} /* namespace sharemind { */

#endif /* SHAREMIND_LIBAS_ASSEMBLE_H */
