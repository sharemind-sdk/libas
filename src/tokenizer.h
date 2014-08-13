/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBAS_TOKENIZER_H
#define SHAREMIND_LIBAS_TOKENIZER_H

#include <string.h>
#include "tokens.h"


#ifdef __cplusplus
extern "C" {
#endif


SharemindAssemblerTokens * sharemind_assembler_tokenize(const char * program,
                            size_t length,
                            size_t * errorSl,
                            size_t *errorSc)
    __attribute__ ((nonnull(1), warn_unused_result));


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBAS_TOKENIZER_H */
