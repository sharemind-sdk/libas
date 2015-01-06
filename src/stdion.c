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

#include "stdion.h"

#include <assert.h>


int sharemind_assembler_fnputs(const char * s, size_t len, FILE * stream) {
    assert(s);
    assert(stream);

    int r = 0;
    for (; len; s++, len--) {
        r = fputc(*s, stream);
        if (r < 0)
            break;
    }
    return r;
}

int sharemind_assembler_nputs(const char * s, size_t len) {
    assert(s);

    int r = 0;
    for (; len; s++, len--) {
        r = putchar(*s);
        if (r < 0)
            break;
    }
    return r;
}
