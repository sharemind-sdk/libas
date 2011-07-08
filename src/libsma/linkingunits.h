/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMA_LINKINGUNITS_H
#define LIBSMA_LINKINGUNITS_H

#include <stddef.h>
#include "../codeblock.h"
#include "../vector.h"

#ifdef __cplusplus
extern "C" {
#endif

enum SMA_Section_Type {
    SMA_SECTION_TYPE_TEXT = 0,
    SMA_SECTION_TYPE_RODATA = 1,
    SMA_SECTION_TYPE_DATA = 2,
    SMA_SECTION_TYPE_BSS = 3,
    SMA_SECTION_TYPE_BIND = 4,
    SMA_SECTION_TYPE_DEBUG = 5,
    SMA_SECTION_TYPE_COUNT = 6
};

struct SMA_Section {
    size_t length;
    union {
        char * data;
        union SVM_IBlock * cbdata;
    };
};

void SMA_Section_init(struct SMA_Section * s);
void SMA_Section_destroy(struct SMA_Section * s);


struct SMA_LinkingUnit {
    struct SMA_Section sections[SMA_SECTION_TYPE_COUNT];
};

void SMA_LinkingUnit_init(struct SMA_LinkingUnit * lu);
void SMA_LinkingUnit_destroy(struct SMA_LinkingUnit * lu);

SVM_VECTOR_DECLARE(SMA_LinkingUnits,struct SMA_LinkingUnit,)

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMA_LINKINGUNITS_H */
