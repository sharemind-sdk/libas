/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMAS_LINKER_H
#define LIBSMAS_LINKER_H

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

struct SMAS_LinkingUnits;

char * SMAS_link(unsigned version, struct SMAS_LinkingUnits * lus, size_t * length, unsigned activeLinkingUnit);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMAS_LINKER_H */
