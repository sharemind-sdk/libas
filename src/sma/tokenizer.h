#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

struct Tokens;

struct Tokens * tokenize(const char * program, size_t length, size_t * errorSl, size_t *errorSc);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif
