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


void SharemindAssemblerSection_init(SharemindAssemblerSection * s) {
    assert(s);

    s->length = 0u;
    s->data = NULL;
}

void SharemindAssemblerSection_destroy(SharemindAssemblerSection * s) {
    assert(s);

    free(s->data);
}

void SharemindAssemblerLinkingUnit_init(SharemindAssemblerLinkingUnit * lu) {
    assert(lu);

    for (size_t i = 0u; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT; i++)
        SharemindAssemblerSection_init(&lu->sections[i]);
}

void SharemindAssemblerLinkingUnit_destroy(SharemindAssemblerLinkingUnit * lu) {
    assert(lu);

    for (size_t i = 0u; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT; i++)
        SharemindAssemblerSection_destroy(&lu->sections[i]);
}

SHAREMIND_VECTOR_DEFINE(SharemindAssemblerLinkingUnits,SharemindAssemblerLinkingUnit,malloc,free,realloc,)
