#ifndef STDION_H
#define STDION_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int fnputs(const char * s, size_t len, FILE * stream);

int nputs(const char * s, size_t len);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* STDION_H */
