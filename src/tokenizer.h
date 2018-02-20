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

#ifndef SHAREMIND_LIBAS_TOKENIZER_H
#define SHAREMIND_LIBAS_TOKENIZER_H

#include <cstring>
#include "tokens.h"


#ifdef __cplusplus
extern "C" {
#endif


SharemindAssemblerTokens * sharemind_assembler_tokenize(const char * program,
                            size_t length,
                            size_t * errorSl,
                            size_t *errorSc)
    __attribute__ ((nonnull(1), warn_unused_result));


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBAS_TOKENIZER_H */
