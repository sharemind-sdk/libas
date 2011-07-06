#ifndef LINKER_H
#define LINKER_H

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

struct SMA_LinkingUnits;

char * SMA_link(unsigned version, struct SMA_LinkingUnits * lus, size_t * length, unsigned activeLinkingUnit);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LINKER_H */
