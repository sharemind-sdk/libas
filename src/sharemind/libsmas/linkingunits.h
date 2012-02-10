/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMAS_LINKINGUNITS_H
#define SHAREMIND_LIBSMAS_LINKINGUNITS_H

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
} SMAS_Section;

void SMAS_Section_init(SMAS_Section * s) __attribute__ ((nonnull(1)));
void SMAS_Section_destroy(SMAS_Section * s) __attribute__ ((nonnull(1)));


typedef struct {
    SMAS_Section sections[SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT];
} SMAS_LinkingUnit;

void SMAS_LinkingUnit_init(SMAS_LinkingUnit * lu) __attribute__ ((nonnull(1)));
void SMAS_LinkingUnit_destroy(SMAS_LinkingUnit * lu) __attribute__ ((nonnull(1)));

SM_VECTOR_DECLARE(SMAS_LinkingUnits,SMAS_LinkingUnit,,)


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMAS_LINKINGUNITS_H */
