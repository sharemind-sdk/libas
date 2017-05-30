/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
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

SHAREMIND_VECTOR_DEFINE_INIT(SharemindAssemblerLinkingUnits,)
SHAREMIND_VECTOR_DEFINE_DESTROY_WITH(
        SharemindAssemblerLinkingUnits,,,
        free,
        SharemindAssemblerLinkingUnit_destroy(value);)
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindAssemblerLinkingUnits,,
                                     realloc)
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindAssemblerLinkingUnits,)
