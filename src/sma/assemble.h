#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include <stddef.h>
#include "../codeblock.h"
#include "../preprocessor.h"
#include "../vector.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Tokens;

enum Section_Type {
    SECTION_TYPE_TEXT = 0,
    SECTION_TYPE_RODATA = 1,
    SECTION_TYPE_DATA = 2,
    SECTION_TYPE_BSS = 3,
    SECTION_TYPE_BIND = 4,
    SECTION_TYPE_DEBUG = 5,
    SECTION_TYPE_COUNT = 6
};

struct Section {
    size_t length;
    union {
        char * data;
        union SVM_IBlock * cbdata;
    };
};

void Section_init(struct Section * s);
void Section_destroy(struct Section * s);


struct LinkingUnit {
    struct Section sections[SECTION_TYPE_COUNT];
};

void LinkingUnit_init(struct LinkingUnit * lu);
void LinkingUnit_destroy(struct LinkingUnit * lu);

SVM_VECTOR_DECLARE(LinkingUnits,struct LinkingUnit,)

#define SMA_ENUM_Assemble_Error \
    ((SMA_ASSEMBLE_OK, = 0)) \
    ((SMA_ASSEMBLE_OUT_OF_MEMORY,)) \
    ((SMA_ASSEMBLE_UNEXPECTED_TOKEN,)) \
    ((SMA_ASSEMBLE_UNEXPECTED_EOF,)) \
    ((SMA_ASSEMBLE_DUPLICATE_LABEL,)) \
    ((SMA_ASSEMBLE_UNKNOWN_DIRECTIVE,)) \
    ((SMA_ASSEMBLE_UNKNOWN_INSTRUCTION,)) \
    ((SMA_ASSEMBLE_INVALID_PARAMETER,)) \
    ((SMA_ASSEMBLE_UNDEFINED_LABEL,))
SVM_ENUM_CUSTOM_DEFINE(SMA_Assemble_Error, SMA_ENUM_Assemble_Error);
SVM_ENUM_DECLARE_TOSTRING(SMA_Assemble_Error);


enum SMA_Assemble_Error SMA_assemble(const struct Tokens * ts, struct LinkingUnits * lus);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* ASSEMBLE_H */
