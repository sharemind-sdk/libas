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
#include <sharemind/comma.h>
#include <sharemind/libexecutable/libexecutable.h>
#include <sharemind/libexecutable/libexecutable_0x0.h>
#include <stdbool.h>
#include <stdlib.h>

#include <stdio.h>

/* COMMON */
static const size_t extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

static inline bool writeSection_0x0(SharemindAssemblerSection * s,
                                    char ** pos,
                                    SHAREMIND_EXECUTABLE_SECTION_TYPE type)
{
    assert(s->length > 0u
           && (s->data || type == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS));

    /* Check for unsupported output format. */
    if (type >= SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0)
        return false;

    /* Write header: */
    assert(s->length <= UINT32_MAX / 8); /** \todo Enforce this */
    if (s->length > UINT32_MAX / 8)
        return false;
    const uint32_t l = (uint32_t) s->length;

    {
        SharemindExecutableSectionHeader0x0 h;
        SharemindExecutableSectionHeader0x0_init(&h, type, l);
        __builtin_memcpy(*pos, &h, sizeof(h));
        (*pos) += sizeof(h);
    }

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

    return true;
}

SHAREMIND_VECTOR_DECLARE_FOREACH(
        SharemindAssemblerLinkingUnits,
        totalSize,
        static inline size_t,
        const,,)
SHAREMIND_VECTOR_DEFINE_FOREACH(
        SharemindAssemblerLinkingUnits,
        totalSize,
        static inline size_t,
        const,
        SharemindAssemblerLinkingUnit,,
        size_t totalSize = 0u;,
        totalSize,
        for (size_t i = 0u; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0;i++)
            if (value->sections[i].length > 0u
                && (value->sections[i].data != NULL
                    || i == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS))
            {
                totalSize += sizeof(SharemindExecutableSectionHeader0x0);
                if (i != SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
                    if (i == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT) {
                        totalSize += value->sections[i].length
                                     * sizeof(SharemindCodeBlock);
                    } else {
                        totalSize +=
                                value->sections[i].length
                                + extraPadding[value->sections[i].length % 8];
                    }
                }
            })
SHAREMIND_VECTOR_DECLARE_FOREACH(SharemindAssemblerLinkingUnits,
                                 writeTo,
                                 static inline bool,
                                 const,
                                 SHAREMIND_COMMA char ** pos,)
SHAREMIND_VECTOR_DEFINE_FOREACH(
        SharemindAssemblerLinkingUnits,
        writeTo,
        static inline bool,
        const,
        SharemindAssemblerLinkingUnit,
        SHAREMIND_COMMA char ** pos,
        char * p = (*pos);,
        ((*pos) = p SHAREMIND_COMMA true),
        /* Calculate number of sections: */
        uint8_t sections = 0u;;
        for (size_t i = 0u; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0;i++)
            if (value->sections[i].length > 0u
                && (value->sections[i].data
                    || i == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS))
                sections++;
        assert(sections > 0u);
        sections--;

        /* Write unit header */
        {
            SharemindExecutableUnitHeader0x0 h;
            SharemindExecutableUnitHeader0x0_init(&h, sections);
            __builtin_memcpy(p, &h, sizeof(h));
            p += sizeof(h);
        }

        /* Write sections: */
        for (size_t i = 0u; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0;i++)
            if (value->sections[i].length > 0u
                && (value->sections[i].data
                    || i == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS))
                if (!writeSection_0x0(&value->sections[i],
                                      &p,
                                      (SHAREMIND_EXECUTABLE_SECTION_TYPE) i))
                    return ((*pos) = p, false);)

static bool sharemind_assembler_link_0x0(char ** data,
                                         SharemindAssemblerLinkingUnits * lus,
                                         size_t * length,
                                         uint8_t activeLinkingUnit)
{
    assert(lus->size > 0u);
    assert(lus->size - 1 <= UINT8_MAX); /** \todo Enforce this */
    if (lus->size - 1 > UINT8_MAX)
        return false;

    /* Resize data: */
    size_t const newLength =
            *length
            + sizeof(SharemindExecutableHeader0x0)
            + (lus->size * sizeof(SharemindExecutableUnitHeader0x0))
            + SharemindAssemblerLinkingUnits_totalSize(lus);
    char * newData = (char *) realloc(*data, newLength);
    if (!newData)
        return false;
    (*data) = newData;

    char * writePtr = newData + (*length);
    {
        SharemindExecutableHeader0x0 h;
        SharemindExecutableHeader0x0_init(&h,
                                          (uint8_t) (lus->size - 1),
                                          activeLinkingUnit);
        __builtin_memcpy(writePtr, &h, sizeof(h));
        writePtr += + sizeof(h);
    }

    const bool r = SharemindAssemblerLinkingUnits_writeTo(lus, &writePtr);
    if (r) {
        assert(writePtr == *data + newLength);
        (*length) = newLength;
    }
    return r;
}

void * sharemind_assembler_link(uint16_t version,
                                SharemindAssemblerLinkingUnits * lus,
                                size_t * length,
                                uint8_t activeLinkingUnit)
{
    assert(lus);
    assert(length);

    if (version > 0u)
        return NULL;

    char * data = (char *) malloc(sizeof(SharemindExecutableCommonHeader));
    if (!data)
        return NULL;
    {
        SharemindExecutableCommonHeader h;
        SharemindExecutableCommonHeader_init(&h, version);
        __builtin_memcpy(data, &h, sizeof(h));
    }
    size_t len = sizeof(SharemindExecutableCommonHeader);
    if (sharemind_assembler_link_0x0(&data, lus, &len, activeLinkingUnit)) {
        (*length) = len;
        return data;
    } else {
        free(data);
        return NULL;
    }
}
