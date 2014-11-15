/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBAS_LINKINGUNITS_H
#define SHAREMIND_LIBAS_LINKINGUNITS_H

#include <sharemind/codeblock.h>
#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/libexecutable/libexecutable_0x0.h>
#include <sharemind/libexecutable/sharemind_executable_section_type.h>
#include <sharemind/vector.h>
#include <stddef.h>
#include <stdlib.h>


SHAREMIND_EXTERN_C_BEGIN

typedef struct {
    size_t length;
    void * data;
} SharemindAssemblerSection;

void SharemindAssemblerSection_init(SharemindAssemblerSection * s)
        __attribute__ ((nonnull(1)));
void SharemindAssemblerSection_destroy(SharemindAssemblerSection * s)
        __attribute__ ((nonnull(1)));


typedef struct {
    SharemindAssemblerSection sections[SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT];
} SharemindAssemblerLinkingUnit;

void SharemindAssemblerLinkingUnit_init(SharemindAssemblerLinkingUnit * lu)
        __attribute__ ((nonnull(1)));
void SharemindAssemblerLinkingUnit_destroy(SharemindAssemblerLinkingUnit * lu)
        __attribute__ ((nonnull(1)));

SHAREMIND_VECTOR_DEFINE_BODY(SharemindAssemblerLinkingUnits,
                             SharemindAssemblerLinkingUnit,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindAssemblerLinkingUnits,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindAssemblerLinkingUnits, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindAssemblerLinkingUnits,
                                 inline,,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY_WITH(
        SharemindAssemblerLinkingUnits,
        inline,
        SharemindAssemblerLinkingUnit,,
        free,
        SharemindAssemblerLinkingUnit_destroy(value);)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindAssemblerLinkingUnits,
                                      inline,
                                      SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindAssemblerLinkingUnits,
                                     inline,
                                     SharemindAssemblerLinkingUnit,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindAssemblerLinkingUnits,
                              inline,
                              SharemindAssemblerLinkingUnit,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindAssemblerLinkingUnits,
                             inline,
                             SharemindAssemblerLinkingUnit)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBAS_LINKINGUNITS_H */
