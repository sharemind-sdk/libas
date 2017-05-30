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

SHAREMIND_VECTOR_DECLARE_BODY(SharemindAssemblerLinkingUnits,
                             SharemindAssemblerLinkingUnit)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindAssemblerLinkingUnits,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindAssemblerLinkingUnits,,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindAssemblerLinkingUnits,,,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindAssemblerLinkingUnits,,
                                      SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindAssemblerLinkingUnits,,
                              SHAREMIND_COMMA visibility("internal"))

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBAS_LINKINGUNITS_H */
