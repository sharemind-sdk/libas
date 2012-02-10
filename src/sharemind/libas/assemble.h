/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBAS_ASSEMBLE_H
#define SHAREMIND_LIBAS_ASSEMBLE_H

#include <sharemind/preprocessor.h>
#include "linkingunits.h"
#include "tokens.h"


#ifdef __cplusplus
extern "C" {
#endif


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
        const SharemindAssemblerTokens * ts,
        SharemindAssemblerLinkingUnits * lus,
        const SharemindAssemblerToken ** errorToken,
        char ** errorString)
    __attribute__ ((nonnull(1, 2), warn_unused_result));


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBAS_ASSEMBLE_H */
