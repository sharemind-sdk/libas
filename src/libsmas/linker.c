/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "linker.h"

#include <assert.h>
#include <stdlib.h>
#include "../codeblock.h"
#include "../libsme/libsme.h"
#include "../libsme/libsme_0x0.h"
#include "linkingunits.h"


/* COMMON */

SM_VECTOR_DEFINE_FOREACH_WITH(SMAS_LinkingUnits,struct SMAS_LinkingUnit,sizetPointer,size_t *,size_t * l,l,)
SM_VECTOR_DEFINE_FOREACH_WITH(SMAS_LinkingUnits,struct SMAS_LinkingUnit,outputPointer,void **,void ** p,p,)

static int SMAS_link_0x0(void ** data, struct SMAS_LinkingUnits * lus, size_t * length, uint8_t activeLinkingUnit);

char * SMAS_link(uint16_t version, struct SMAS_LinkingUnits * lus, size_t * length, uint8_t activeLinkingUnit) {
    if (version > 0u)
        return NULL;

    *length = sizeof(struct SME_Common_Header);
    void * data = malloc(sizeof(struct SME_Common_Header));
    if (!data)
        return NULL;
    SME_Common_Header_init(data, version);

    if (SMAS_link_0x0(&data, lus, length, activeLinkingUnit)) {
        return data;
    } else {
        free(data);
        *length = 0u;
        return NULL;
    }
}


static const size_t extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

static int calculateLinkingUnitSize_0x0(struct SMAS_LinkingUnit * lu, size_t * s) {
    for (unsigned i = 0u; i < SME_SECTION_TYPE_COUNT_0x0; i++)
        if (lu->sections[i].length > 0u && lu->sections[i].data != NULL) {
            if (i == SME_SECTION_TYPE_TEXT) {
                *s += sizeof(struct SME_Section_Header_0x0) + lu->sections[i].length * sizeof(union SM_CodeBlock);
            } else {
                *s += sizeof(struct SME_Section_Header_0x0) + lu->sections[i].length + extraPadding[lu->sections[i].length % 8];
            }
        }
    return 1;
}

static int writeSection_0x0(struct SMAS_Section * s, void ** pos, enum SME_Section_Type type) {
    assert(s->length > 0u && s->data != NULL);

    /* Check for unsupported output format. */
    if (type >= SME_SECTION_TYPE_COUNT_0x0)
        return 0;

    /* Write header: */
    uint32_t l = s->length; /** \todo check cast overflow? */
    SME_Section_Header_0x0_init((struct SME_Section_Header_0x0 *) *pos, type, l);
    (*pos) += sizeof(struct SME_Section_Header_0x0);

    if (type == SME_SECTION_TYPE_TEXT) {
        /* Write section data */
        __builtin_memcpy(*pos, s->data, l * 8); /** \todo check multiplication overflow? */
        (*pos) += l * 8;
    } else {
        /* Write section data */
        __builtin_memcpy(*pos, s->data, l);
        (*pos) += l;

        /* Extra padding: */
        __builtin_bzero(*pos, extraPadding[l % 8]);
        (*pos) += extraPadding[l % 8];
    }

    return 1;
}

static int writeLinkingUnit_0x0(struct SMAS_LinkingUnit * lu, void ** pos) {
    /* Calculate number of sections: */
    uint8_t sections = 0u;
    for (unsigned i = 0u; i < SME_SECTION_TYPE_COUNT_0x0; i++)
        if (lu->sections[i].length > 0u && lu->sections[i].data != NULL)
            sections++;
    assert(sections > 0u);
    sections--;

    /* Write unit header */
    SME_Unit_Header_0x0_init((struct SME_Unit_Header_0x0 *) *pos, sections);
    (*pos) += sizeof(struct SME_Unit_Header_0x0);

    /* Write sections: */
    for (unsigned i = 0u; i < SME_SECTION_TYPE_COUNT_0x0; i++)
        if (lu->sections[i].length > 0u && lu->sections[i].data != NULL)
            if (!writeSection_0x0(&lu->sections[i], pos, i))
                return 0;

    return 1;
}

static int SMAS_link_0x0(void ** data, struct SMAS_LinkingUnits * lus, size_t * length, uint8_t activeLinkingUnit) {
    assert(lus->size > 0u);

    size_t oldLength = *length;

    /* Resize data: */
    (*length) += sizeof(struct SME_Header_0x0) + (lus->size * sizeof(struct SME_Unit_Header_0x0));
    int r = SMAS_LinkingUnits_foreach_with_sizetPointer(lus, &calculateLinkingUnitSize_0x0, length);
    assert(r);
    void * newData = realloc(*data, *length);
    if (!newData)
        return 0;
    *data = newData;

    void * writePtr = newData + oldLength;
    SME_Header_0x0_init((struct SME_Header_0x0 *) writePtr, lus->size - 1, activeLinkingUnit);
    writePtr += sizeof(struct SME_Header_0x0);

    r = SMAS_LinkingUnits_foreach_with_outputPointer(lus, &writeLinkingUnit_0x0, &writePtr);
    if (!r)
        return 0;

    assert(writePtr == (*data + *length));

    return 1;
}
