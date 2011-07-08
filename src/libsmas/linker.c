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
#include "../static_assert.h"
#include "linkingunits.h"


/* COMMON */

SM_VECTOR_DEFINE_FOREACH_WITH(SMAS_LinkingUnits,struct SMAS_LinkingUnit,sizetPointer,size_t *,size_t * l,l)
SM_VECTOR_DEFINE_FOREACH_WITH(SMAS_LinkingUnits,struct SMAS_LinkingUnit,outputPointer,void **,void ** p,p)

struct SME_Common_Header {
    char magic[32];
    uint64_t byte_order_verification __attribute__ ((packed));
    uint16_t file_format_version __attribute__ ((packed));
};
SM_STATIC_ASSERT(sizeof(struct SME_Common_Header) == 32 + 8 + 2);

static void SME_Common_Header_init(struct SME_Common_Header * h, uint16_t version) {
    static const char magic[32] = "Sharemind Executable";
    __builtin_memcpy(h->magic, magic, 32);
    h->byte_order_verification = 0x0123456789abcdef;
    h->file_format_version = version;
}

static char * SMAS_link_0x0(struct SMAS_LinkingUnits * lus, size_t * length, unsigned activeLinkingUnit);

char * SMAS_link(unsigned version, struct SMAS_LinkingUnits * lus, size_t * length, unsigned activeLinkingUnit) {
    if (version > 0u)
        return NULL;

    *length = sizeof(struct SME_Common_Header);
    return SMAS_link_0x0(lus, length, activeLinkingUnit);
}


/* VERSION 0x0 */

struct SME_Header_0x0 {
    uint8_t number_of_units_minus_one;
    uint8_t active_linking_unit;
    uint8_t zeroPadding[4];
};
SM_STATIC_ASSERT(sizeof(struct SME_Header_0x0) == 1u + 1u + 4u);

struct SME_Unit_Header_0x0 {
    char type[32];
    uint8_t sections_minus_one;
    uint8_t zeroPadding[7];
};
SM_STATIC_ASSERT(sizeof(struct SME_Unit_Header_0x0) == 32u + 1u + 7u);

struct SME_Section_Header_0x0 {
    char type[32];
    uint32_t length __attribute__ ((packed));
    uint8_t zeroPadding[4];
};
SM_STATIC_ASSERT(sizeof(struct SME_Section_Header_0x0) == 32u + 4u + 4u);


static const size_t extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

static int calculateLinkingUnitSize_0x0(struct SMAS_LinkingUnit * lu, size_t * s) {
    for (unsigned i = 0u; i < SMAS_SECTION_TYPE_COUNT; i++)
        if (lu->sections[i].length > 0u && lu->sections[i].data != NULL) {
            if (i == SMAS_SECTION_TYPE_TEXT) {
                *s += sizeof(struct SME_Section_Header_0x0) + lu->sections[i].length * sizeof(union SM_CodeBlock);
            } else {
                *s += sizeof(struct SME_Section_Header_0x0) + lu->sections[i].length + extraPadding[lu->sections[i].length % 8];
            }
        }
    return 1;
}

static int writeSection_0x0(struct SMAS_Section * s, void ** pos, enum SMAS_Section_Type type) {
    assert(s->length > 0u && s->data != NULL);

    /* Write magic: */
    assert(type < SMAS_SECTION_TYPE_COUNT);
    static const char magic[SMAS_SECTION_TYPE_COUNT][32] = {
        "TEXT", "RODATA", "DATA", "BSS", "BIND", "DEBUG"
    };
    __builtin_memcpy(*pos, magic[type], 32);
    (*pos) += 32;

    /* Write section length */
    uint32_t l = s->length; /** \todo check cast overflow? */
    __builtin_memcpy(*pos, &l, 4);
    (*pos) += 4;

    /* Extra padding: */
    __builtin_bzero(*pos, 4);
    (*pos) += 4;

    if (type == SMAS_SECTION_TYPE_TEXT) {
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

    /* Write magic: */
    static const char magic[32] = "Linking Unit";
    __builtin_memcpy(*pos, magic, 32);
    (*pos) += 32;

    /* Calculate and write number of sections: */
    uint8_t sections = 0u;
    for (unsigned i = 0u; i < SMAS_SECTION_TYPE_COUNT; i++)
        if (lu->sections[i].length > 0u && lu->sections[i].data != NULL)
            sections++;
    assert(sections > 0u);
    sections--;
    __builtin_memcpy(*pos, &sections, 1);
    (*pos)++;

    /* Extra padding: */
    __builtin_bzero(*pos, 7);
    (*pos) += 7;

    /* Write sections: */
    for (unsigned i = 0u; i < SMAS_SECTION_TYPE_COUNT; i++)
        if (lu->sections[i].length > 0u && lu->sections[i].data != NULL)
            writeSection_0x0(&lu->sections[i], pos, i);

    return 1;
}

static char * SMAS_link_0x0(struct SMAS_LinkingUnits * lus, size_t * length, unsigned activeLinkingUnit) {
    assert(lus->size > 0u);

    (*length) += sizeof(struct SME_Header_0x0) + (lus->size * sizeof(struct SME_Unit_Header_0x0));
    int r = SMAS_LinkingUnits_foreach_with_sizetPointer(lus, &calculateLinkingUnitSize_0x0, length);
    assert(r);

    void * data = malloc(*length);
    SME_Common_Header_init(data, 0x0u);
    void * c = data + sizeof(struct SME_Common_Header);

    ((struct SME_Header_0x0 *) c)->number_of_units_minus_one = lus->size - 1;
    ((struct SME_Header_0x0 *) c)->active_linking_unit = activeLinkingUnit;
    __builtin_bzero(((struct SME_Header_0x0 *) c)->zeroPadding, 4);
    c += sizeof(struct SME_Header_0x0);

    r = SMAS_LinkingUnits_foreach_with_outputPointer(lus, &writeLinkingUnit_0x0, &c);
    assert(r);
    assert(c == (data + *length));

    return data;
}
