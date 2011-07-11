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

struct SMAS_Section {
    size_t length;
    union {
        char * data;
        union SM_CodeBlock * cbdata;
    };
};

void SMAS_Section_init(struct SMAS_Section * s);
void SMAS_Section_destroy(struct SMAS_Section * s);


struct SMAS_LinkingUnit {
    struct SMAS_Section sections[SME_SECTION_TYPE_COUNT];
};

void SMAS_LinkingUnit_init(struct SMAS_LinkingUnit * lu);
void SMAS_LinkingUnit_destroy(struct SMAS_LinkingUnit * lu);

SM_VECTOR_DECLARE(SMAS_LinkingUnits,struct SMAS_LinkingUnit,)

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMAS_LINKINGUNITS_H */
