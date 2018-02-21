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

#include <sharemind/preprocessor.h>
#include "linkingunits.h"
#include "tokens.h"


#define SHAREMIND_ASSEMBLER_ERROR_ENUM \
    ((SHAREMIND_ASSEMBLE_OK, = 0)) \
    ((SHAREMIND_ASSEMBLE_OUT_OF_MEMORY,)) \
    ((SHAREMIND_ASSEMBLE_UNEXPECTED_TOKEN,)) \
    ((SHAREMIND_ASSEMBLE_UNEXPECTED_EOF,)) \
    ((SHAREMIND_ASSEMBLE_DUPLICATE_LABEL,)) \
    ((SHAREMIND_ASSEMBLE_UNKNOWN_DIRECTIVE,)) \
    ((SHAREMIND_ASSEMBLE_UNKNOWN_INSTRUCTION,)) \
    ((SHAREMIND_ASSEMBLE_INVALID_NUMBER_OF_PARAMETERS,)) \
    ((SHAREMIND_ASSEMBLE_INVALID_PARAMETER,)) \
    ((SHAREMIND_ASSEMBLE_UNDEFINED_LABEL,)) \
    ((SHAREMIND_ASSEMBLE_INVALID_LABEL,)) \
    ((SHAREMIND_ASSEMBLE_INVALID_LABEL_OFFSET,))
SHAREMIND_ENUM_CUSTOM_DEFINE(SharemindAssemblerError, SHAREMIND_ASSEMBLER_ERROR_ENUM);
SHAREMIND_ENUM_DECLARE_TOSTRING(SharemindAssemblerError);

SharemindAssemblerError sharemind_assembler_assemble(
        sharemind::AssemblerTokens const & ts,
        SharemindAssemblerLinkingUnits * lus,
        sharemind::AssemblerTokens::const_iterator * errorToken,
        char ** errorString)
    __attribute__ ((nonnull(2), warn_unused_result));

#endif /* SHAREMIND_LIBAS_ASSEMBLE_H */
