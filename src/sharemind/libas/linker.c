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
#include <sharemind/codeblock.h>
#include <sharemind/libexecutable/libexecutable.h>
#include <sharemind/libexecutable/libexecutable_0x0.h>
#include <stdlib.h>


/* COMMON */

SHAREMIND_VECTOR_DECLARE_FOREACH_WITH(SharemindAssemblerLinkingUnits,SharemindAssemblerLinkingUnit,sizetPointer,size_t *,)
SHAREMIND_VECTOR_DEFINE_FOREACH_WITH(SharemindAssemblerLinkingUnits,SharemindAssemblerLinkingUnit,sizetPointer,size_t *,size_t * l,l,)
SHAREMIND_VECTOR_DECLARE_FOREACH_WITH(SharemindAssemblerLinkingUnits,SharemindAssemblerLinkingUnit,outputPointer,uint8_t **,)
SHAREMIND_VECTOR_DEFINE_FOREACH_WITH(SharemindAssemblerLinkingUnits,SharemindAssemblerLinkingUnit,outputPointer,uint8_t **,uint8_t ** p,p,)

static int sharemind_assembler_link_0x0(SharemindExecutableCommonHeader ** data,
                                        SharemindAssemblerLinkingUnits * lus,
                                        size_t * length,
                                        uint8_t activeLinkingUnit);

uint8_t * sharemind_assembler_link(uint16_t version, SharemindAssemblerLinkingUnits * lus, size_t * length, uint8_t activeLinkingUnit) {
    assert(lus);
    assert(length);

    if (version > 0u)
        return NULL;

    (*length) = sizeof(SharemindExecutableCommonHeader);
    SharemindExecutableCommonHeader * data = (SharemindExecutableCommonHeader *) malloc(sizeof(SharemindExecutableCommonHeader));
    if (!data)
        return NULL;
    SharemindExecutableCommonHeader_init(data, version);

    if (sharemind_assembler_link_0x0(&data, lus, length, activeLinkingUnit)) {
        return (uint8_t *) data;
    } else {
        free(data);
        (*length) = 0u;
        return NULL;
    }
}


static const size_t extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

static int calculateLinkingUnitSize_0x0(SharemindAssemblerLinkingUnit * lu, size_t * s) {
    for (unsigned i = 0u; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0; i++)
        if (lu->sections[i].length > 0u && (lu->sections[i].data != NULL || i == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS)) {
            *s += sizeof(SharemindExecutableSectionHeader0x0);
            if (i != SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
                if (i == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT) {
                    *s += lu->sections[i].length * sizeof(SharemindCodeBlock);
                } else {
                    *s += lu->sections[i].length + extraPadding[lu->sections[i].length % 8];
                }
            }
        }
    return 1;
}

static int writeSection_0x0(SharemindAssemblerSection * s, uint8_t ** pos, SHAREMIND_EXECUTABLE_SECTION_TYPE type) {
    assert(s->length > 0u && (s->data != NULL || type == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS));

    /* Check for unsupported output format. */
    if (type >= SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0)
        return 0;

    /* Write header: */
    assert(s->length <= UINT32_MAX / 8); /** \todo Enforce this */
    if (s->length > UINT32_MAX / 8)
        return 0;
    uint32_t l = (uint32_t) s->length;

    SharemindExecutableSectionHeader0x0_init((SharemindExecutableSectionHeader0x0 *) *pos, type, l);
    (*pos) += sizeof(SharemindExecutableSectionHeader0x0);

    if (type == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT) {
        /* Write section data */
        __builtin_memcpy(*pos, s->data, l * 8);
        (*pos) += l * 8;
    } else if (type != SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
        /* Write section data */
        __builtin_memcpy(*pos, s->data, l);
        (*pos) += l;

        /* Extra padding: */
        __builtin_bzero(*pos, extraPadding[l % 8]);
        (*pos) += extraPadding[l % 8];
    }

    return 1;
}

static int writeLinkingUnit_0x0(SharemindAssemblerLinkingUnit * lu, uint8_t ** pos) {
    /* Calculate number of sections: */
    uint8_t sections = 0u;
    for (unsigned i = 0u; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0; i++)
        if (lu->sections[i].length > 0u && (lu->sections[i].data != NULL || i == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS))
            sections++;
    assert(sections > 0u);
    sections--;

    /* Write unit header */
    SharemindExecutableUnitHeader0x0_init((SharemindExecutableUnitHeader0x0 *) *pos, sections);
    (*pos) += sizeof(SharemindExecutableUnitHeader0x0);

    /* Write sections: */
    for (int i = 0; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0; i++)
        if (lu->sections[i].length > 0u && (lu->sections[i].data != NULL || i == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS))
            if (!writeSection_0x0(&lu->sections[i], pos, (SHAREMIND_EXECUTABLE_SECTION_TYPE) i))
                return 0;

    return 1;
}

static int sharemind_assembler_link_0x0(SharemindExecutableCommonHeader ** data, SharemindAssemblerLinkingUnits * lus, size_t * length, uint8_t activeLinkingUnit) {
    assert(lus->size > 0u);

    size_t oldLength = *length;

    /* Resize data: */
    (*length) += sizeof(SharemindExecutableHeader0x0) + (lus->size * sizeof(SharemindExecutableUnitHeader0x0));
    int r = SharemindAssemblerLinkingUnits_foreach_with_sizetPointer(lus, &calculateLinkingUnitSize_0x0, length);
    assert(r);
    SharemindExecutableCommonHeader * newData = (SharemindExecutableCommonHeader *) realloc((void *) *data, *length);
    if (!newData)
        return 0;
    (*data) = newData;

    uint8_t * writePtr = ((uint8_t *) newData) + oldLength;
    assert(lus->size - 1 <= UINT8_MAX); /** \todo Enforce this */
    if (lus->size - 1 > UINT8_MAX)
        return 0;

    SharemindExecutableHeader0x0_init((SharemindExecutableHeader0x0 *) writePtr, (uint8_t) (lus->size - 1), activeLinkingUnit);
    writePtr += sizeof(SharemindExecutableHeader0x0);

    r = SharemindAssemblerLinkingUnits_foreach_with_outputPointer(lus, &writeLinkingUnit_0x0, &writePtr);
    if (!r)
        return 0;

    assert(writePtr == ((uint8_t *) *data) + (*length));

    return 1;
}
