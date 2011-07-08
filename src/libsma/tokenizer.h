/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMA_TOKENIZER_H
#define LIBSMA_TOKENIZER_H

#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

struct SMA_Tokens;

struct SMA_Tokens * SMA_tokenize(const void * program, size_t length, size_t * errorSl, size_t *errorSc);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMA_TOKENIZER_H */
