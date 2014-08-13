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
#include <sharemind/libexecutable/sharemind_executable_section_type.h>
#include <sharemind/vector.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    size_t length;
    void * data;
} SharemindAssemblerSection;

void SharemindAssemblerSection_init(SharemindAssemblerSection * s) __attribute__ ((nonnull(1)));
void SharemindAssemblerSection_destroy(SharemindAssemblerSection * s) __attribute__ ((nonnull(1)));


typedef struct {
    SharemindAssemblerSection sections[SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT];
} SharemindAssemblerLinkingUnit;

void SharemindAssemblerLinkingUnit_init(SharemindAssemblerLinkingUnit * lu) __attribute__ ((nonnull(1)));
void SharemindAssemblerLinkingUnit_destroy(SharemindAssemblerLinkingUnit * lu) __attribute__ ((nonnull(1)));

SHAREMIND_VECTOR_DECLARE(SharemindAssemblerLinkingUnits,SharemindAssemblerLinkingUnit,,)


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBAS_LINKINGUNITS_H */
