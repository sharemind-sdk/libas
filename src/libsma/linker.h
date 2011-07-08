/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMA_LINKER_H
#define LIBSMA_LINKER_H

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

struct SMA_LinkingUnits;

char * SMA_link(unsigned version, struct SMA_LinkingUnits * lus, size_t * length, unsigned activeLinkingUnit);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMA_LINKER_H */
