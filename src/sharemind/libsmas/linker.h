/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMAS_LINKER_H
#define SHAREMIND_LIBSMAS_LINKER_H

#include <stddef.h>
#include <stdint.h>
#include "linkingunits.h"


#ifdef __cplusplus
extern "C" {
#endif

uint8_t * SMAS_link(uint16_t version,
                    SMAS_LinkingUnits * lus,
                    size_t * length,
                    uint8_t activeLinkingUnit)
    __attribute__ ((nonnull(2, 3), warn_unused_result));

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMAS_LINKER_H */