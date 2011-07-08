/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef STDION_H
#define STDION_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int fnputs(const char * s, size_t len, FILE * stream);

int nputs(const char * s, size_t len);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* STDION_H */
