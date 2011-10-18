/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMAS_TOKENIZER_H
#define LIBSMAS_TOKENIZER_H

#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

struct SMAS_Tokens;

struct SMAS_Tokens * SMAS_tokenize(const char * program, size_t length, size_t * errorSl, size_t *errorSc);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMAS_TOKENIZER_H */
