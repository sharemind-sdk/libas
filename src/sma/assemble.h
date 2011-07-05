#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include <stddef.h>
#include "../codeblock.h"
#include "../preprocessor.h"
#include "../vector.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SMA_Tokens;

enum Section_Type {
    SMA_SECTION_TYPE_TEXT = 0,
    SMA_SECTION_TYPE_RODATA = 1,
    SMA_SECTION_TYPE_DATA = 2,
    SMA_SECTION_TYPE_BSS = 3,
    SMA_SECTION_TYPE_BIND = 4,
    SMA_SECTION_TYPE_DEBUG = 5,
    SMA_SECTION_TYPE_COUNT = 6
};

struct SMA_Section {
    size_t length;
    union {
        char * data;
        union SVM_IBlock * cbdata;
    };
};

void SMA_Section_init(struct SMA_Section * s);
void SMA_Section_destroy(struct SMA_Section * s);


struct SMA_LinkingUnit {
    struct SMA_Section sections[SMA_SECTION_TYPE_COUNT];
};

void SMA_LinkingUnit_init(struct SMA_LinkingUnit * lu);
void SMA_LinkingUnit_destroy(struct SMA_LinkingUnit * lu);

SVM_VECTOR_DECLARE(SMA_LinkingUnits,struct SMA_LinkingUnit,)

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


enum SMA_Assemble_Error SMA_assemble(const struct SMA_Tokens * ts,
                                     struct SMA_LinkingUnits * lus);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* ASSEMBLE_H */
