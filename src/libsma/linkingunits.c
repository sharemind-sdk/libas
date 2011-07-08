/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "linkingunits.h"

#include <stdlib.h>


void SMA_Section_init(struct SMA_Section * s) {
    s->length = 0u;
    s->data = NULL;
}

void SMA_Section_destroy(struct SMA_Section * s) {
    free(s->data);
}

void SMA_LinkingUnit_init(struct SMA_LinkingUnit * lu) {
    for (size_t i = 0u; i < SMA_SECTION_TYPE_COUNT; i++)
        SMA_Section_init(&lu->sections[i]);
}

void SMA_LinkingUnit_destroy(struct SMA_LinkingUnit * lu) {
    for (size_t i = 0u; i < SMA_SECTION_TYPE_COUNT; i++)
        SMA_Section_destroy(&lu->sections[i]);
}

SVM_VECTOR_DEFINE(SMA_LinkingUnits,struct SMA_LinkingUnit,malloc,free,realloc)
