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


void SMAS_Section_init(SMAS_Section * s) {
    assert(s);

    s->length = 0u;
    s->data = NULL;
}

void SMAS_Section_destroy(SMAS_Section * s) {
    assert(s);

    free(s->data);
}

void SMAS_LinkingUnit_init(SMAS_LinkingUnit * lu) {
    assert(lu);

    for (size_t i = 0u; i < SME_SECTION_TYPE_COUNT; i++)
        SMAS_Section_init(&lu->sections[i]);
}

void SMAS_LinkingUnit_destroy(SMAS_LinkingUnit * lu) {
    assert(lu);

    for (size_t i = 0u; i < SME_SECTION_TYPE_COUNT; i++)
        SMAS_Section_destroy(&lu->sections[i]);
}

SM_VECTOR_DEFINE(SMAS_LinkingUnits,SMAS_LinkingUnit,malloc,free,realloc,)
