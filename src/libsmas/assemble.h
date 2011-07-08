/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMAS_ASSEMBLE_H
#define LIBSMAS_ASSEMBLE_H

#include "../preprocessor.h"
#include "linkingunits.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SMAS_Tokens;

#define SMAS_ENUM_Assemble_Error \
    ((SMAS_ASSEMBLE_OK, = 0)) \
    ((SMAS_ASSEMBLE_OUT_OF_MEMORY,)) \
    ((SMAS_ASSEMBLE_UNEXPECTED_TOKEN,)) \
    ((SMAS_ASSEMBLE_UNEXPECTED_EOF,)) \
    ((SMAS_ASSEMBLE_DUPLICATE_LABEL,)) \
    ((SMAS_ASSEMBLE_UNKNOWN_DIRECTIVE,)) \
    ((SMAS_ASSEMBLE_UNKNOWN_INSTRUCTION,)) \
    ((SMAS_ASSEMBLE_INVALID_PARAMETER,)) \
    ((SMAS_ASSEMBLE_UNDEFINED_LABEL,)) \
    ((SMAS_ASSEMBLE_INVALID_LABEL,))
SM_ENUM_CUSTOM_DEFINE(SMAS_Assemble_Error, SMAS_ENUM_Assemble_Error);
SM_ENUM_DECLARE_TOSTRING(SMAS_Assemble_Error);


enum SMAS_Assemble_Error SMAS_assemble(const struct SMAS_Tokens * ts,
                                       struct SMAS_LinkingUnits * lus);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMAS_ASSEMBLE_H */
