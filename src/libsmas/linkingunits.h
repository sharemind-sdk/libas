/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMAS_LINKINGUNITS_H
#define LIBSMAS_LINKINGUNITS_H

#include <stddef.h>
#include "../codeblock.h"
#include "../libsme/libsme.h"
#include "../vector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t length;
    void * data;
} SMAS_Section;

void SMAS_Section_init(SMAS_Section * s);
void SMAS_Section_destroy(SMAS_Section * s);


typedef struct {
    SMAS_Section sections[SME_SECTION_TYPE_COUNT];
} SMAS_LinkingUnit;

void SMAS_LinkingUnit_init(SMAS_LinkingUnit * lu);
void SMAS_LinkingUnit_destroy(SMAS_LinkingUnit * lu);

SM_VECTOR_DECLARE(SMAS_LinkingUnits,SMAS_LinkingUnit,,)

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMAS_LINKINGUNITS_H */
