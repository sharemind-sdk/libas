/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include "../preprocessor.h"
#include "linkingunits.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SMA_Tokens;

#define SMA_ENUM_Assemble_Error \
    ((SMA_ASSEMBLE_OK, = 0)) \
    ((SMA_ASSEMBLE_OUT_OF_MEMORY,)) \
    ((SMA_ASSEMBLE_UNEXPECTED_TOKEN,)) \
    ((SMA_ASSEMBLE_UNEXPECTED_EOF,)) \
    ((SMA_ASSEMBLE_DUPLICATE_LABEL,)) \
    ((SMA_ASSEMBLE_UNKNOWN_DIRECTIVE,)) \
    ((SMA_ASSEMBLE_UNKNOWN_INSTRUCTION,)) \
    ((SMA_ASSEMBLE_INVALID_PARAMETER,)) \
    ((SMA_ASSEMBLE_UNDEFINED_LABEL,)) \
    ((SMA_ASSEMBLE_INVALID_LABEL,))
SVM_ENUM_CUSTOM_DEFINE(SMA_Assemble_Error, SMA_ENUM_Assemble_Error);
SVM_ENUM_DECLARE_TOSTRING(SMA_Assemble_Error);


enum SMA_Assemble_Error SMA_assemble(const struct SMA_Tokens * ts,
                                     struct SMA_LinkingUnits * lus);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* ASSEMBLE_H */
