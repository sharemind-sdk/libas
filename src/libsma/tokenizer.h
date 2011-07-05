#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

struct SMA_Tokens;

struct SMA_Tokens * SMA_tokenize(const void * program, size_t length, size_t * errorSl, size_t *errorSc);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif
