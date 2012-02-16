/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "assemble.h"

#include <assert.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <sharemind/trie.h>
#include <stdio.h>
#include <stdlib.h>


static inline int sharemind_assembler_assign_add_sizet_int64(size_t * lhs, const int64_t rhs) {
    if (rhs > 0) {
        if (((uint64_t) rhs) > SIZE_MAX - (*lhs))
            return 0;
        (*lhs) += (uint64_t) rhs;
    } else if (rhs < 0) {
        if (rhs == INT64_MIN) {
            if ((*lhs) <= (uint64_t) INT64_MAX)
                return 0;
            (*lhs)--;
            (*lhs) -= (uint64_t) INT64_MAX;
        } else {
            if ((*lhs) < (uint64_t) -rhs)
              return 0;
            (*lhs) -= (uint64_t) -rhs;
        }
    }
    return 1;
}

static inline int sharemind_assembler_substract_sizet_sizet_to_int64(int64_t *dest, const size_t lhs, const size_t rhs) {
    if (lhs >= rhs) {
        size_t r = lhs - rhs;
        if (r > INT64_MAX)
            return 0;
        (*dest) = (int64_t) r;
    } else {
        size_t mr = rhs - lhs;
        assert(mr > 0);
        if (mr - 1 > (uint64_t) INT64_MAX) {
            return 0;
        } else if (mr - 1 == (uint64_t) INT64_MAX) {
            (*dest) = INT64_MIN;
        } else {
            (*dest) = -((int64_t) mr);
        }
    }
    return 1;
}


typedef struct {
    size_t linkingUnit;
    int section;
    size_t offset;
} SharemindAssemblerLabelLocation;

SHAREMIND_TRIE_DECLARE(SharemindAssemblerLabelLocations,SharemindAssemblerLabelLocation,)
SHAREMIND_TRIE_DEFINE(SharemindAssemblerLabelLocations,SharemindAssemblerLabelLocation,malloc,free,)


typedef struct {
    size_t linkingUnit;
    int section;
    int64_t extraOffset;
    int doJumpLabel;
    size_t jmpOffset;
    void ** data;
    size_t cbdata_index;
    const SharemindAssemblerToken * token; /* NULL if this slot is already filled */
} SharemindAssemblerLabelSlot;

static int SharemindAssemblerLabelSlot_filled(SharemindAssemblerLabelSlot * s, SharemindAssemblerLabelSlot ** d) {
    assert(s);
    if (s->token == NULL)
        return 1;
    (*d) = s;
    return 0;
}

SHAREMIND_VECTOR_DECLARE(SharemindAssemblerLabelSlots,SharemindAssemblerLabelSlot,,)
SHAREMIND_VECTOR_DEFINE(SharemindAssemblerLabelSlots,SharemindAssemblerLabelSlot,malloc,free,realloc,)
SHAREMIND_VECTOR_DECLARE_FOREACH_WITH(SharemindAssemblerLabelSlots,SharemindAssemblerLabelSlot,labelLocationPointer,SharemindAssemblerLabelLocation *,)
SHAREMIND_VECTOR_DEFINE_FOREACH_WITH(SharemindAssemblerLabelSlots,SharemindAssemblerLabelSlot,labelLocationPointer,SharemindAssemblerLabelLocation *,SharemindAssemblerLabelLocation * p,p,)
SHAREMIND_VECTOR_DECLARE_FOREACH_WITH(SharemindAssemblerLabelSlots,SharemindAssemblerLabelSlot,labelSlotPointerPointer,SharemindAssemblerLabelSlot **,)
SHAREMIND_VECTOR_DEFINE_FOREACH_WITH(SharemindAssemblerLabelSlots,SharemindAssemblerLabelSlot,labelSlotPointerPointer,SharemindAssemblerLabelSlot **,SharemindAssemblerLabelSlot ** p,p,)

static int SharemindAssemblerLabelSlots_all_slots_filled(SharemindAssemblerLabelSlots * ss, SharemindAssemblerLabelSlot ** d) {
    return SharemindAssemblerLabelSlots_foreach_with_labelSlotPointerPointer(ss, &SharemindAssemblerLabelSlot_filled, d);
}

SHAREMIND_TRIE_DECLARE(SharemindAssemblerLabelSlotsTrie,SharemindAssemblerLabelSlots,)
SHAREMIND_TRIE_DEFINE(SharemindAssemblerLabelSlotsTrie,SharemindAssemblerLabelSlots,malloc,free,)
SHAREMIND_TRIE_DECLARE_FOREACH_WITH(SharemindAssemblerLabelSlotsTrie,SharemindAssemblerLabelSlots,labelSlotPointerPointer,SharemindAssemblerLabelSlot **,SharemindAssemblerLabelSlot ** p,)
SHAREMIND_TRIE_DEFINE_FOREACH_WITH(SharemindAssemblerLabelSlotsTrie,SharemindAssemblerLabelSlots,labelSlotPointerPointer,SharemindAssemblerLabelSlot **,SharemindAssemblerLabelSlot ** p,p,)

static int SharemindAssemblerLabelSlot_fill(SharemindAssemblerLabelSlot * s, SharemindAssemblerLabelLocation * l) {
    assert(s);
    assert(s->token);
    assert(l);

    size_t absTarget = l->offset;
    if (!sharemind_assembler_assign_add_sizet_int64(&absTarget, s->extraOffset))
        goto fill_error;

    if (!s->doJumpLabel) { /* Normal absolute label */
        ((SharemindCodeBlock *) *s->data)[s->cbdata_index].uint64[0] = absTarget;
    } else { /* Relative jump label */
        if (s->section != l->section || s->linkingUnit != l->linkingUnit)
            goto fill_error;

        assert(s->section == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT);
        assert(s->jmpOffset < l->offset); /* Because we're one-pass. */

        if (!sharemind_assembler_substract_sizet_sizet_to_int64(&((SharemindCodeBlock *) *s->data)[s->cbdata_index].int64[0], absTarget, s->jmpOffset))
            goto fill_error;

        /** \todo Maybe check whether there's really an instruction there */
    }
    s->token = NULL;
    return 1;

fill_error:
    /** \todo Provide better diagnostics */
    return 0;
}

SHAREMIND_ENUM_CUSTOM_DEFINE_TOSTRING(SharemindAssemblerError, SHAREMIND_ASSEMBLER_ERROR_ENUM);

#define SHAREMIND_ASSEMBLER_ASSEMBLE_EOF_TEST     (unlikely(  t >= e))
#define SHAREMIND_ASSEMBLER_ASSEMBLE_INC_EOF_TEST (unlikely(++t >= e))

#define SHAREMIND_ASSEMBLER_ASSEMBLE_INC_CHECK_EOF(eof) \
    if (SHAREMIND_ASSEMBLER_ASSEMBLE_INC_EOF_TEST) { \
        goto eof; \
    } else (void) 0

#define SHAREMIND_ASSEMBLER_ASSEMBLE_DO_EOL(eof,noexpect) \
    if (1) { \
        if (SHAREMIND_ASSEMBLER_ASSEMBLE_EOF_TEST) \
            goto eof; \
        if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_NEWLINE)) \
            goto noexpect; \
    } else (void) 0

#define SHAREMIND_ASSEMBLER_ASSEMBLE_INC_DO_EOL(eof,noexpect) \
    if (1) { \
        t++; \
        SHAREMIND_ASSEMBLER_ASSEMBLE_DO_EOL(eof,noexpect); \
    } else (void) 0

SharemindAssemblerError sharemind_assembler_assemble(
        const SharemindAssemblerTokens * ts,
        SharemindAssemblerLinkingUnits * lus,
        const SharemindAssemblerToken ** errorToken,
        char ** errorString)
{
    SharemindAssemblerError returnStatus;
    SharemindAssemblerToken * t;
    SharemindAssemblerToken * e;
    SharemindAssemblerLinkingUnit * lu;
    size_t lu_index = 0u;
    int section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
    size_t numBindings = 0u;
    size_t numPdBindings = 0u;
    void * dataToWrite = NULL;
    size_t dataToWriteLength = 0u;

    /* for .data and .fill: */
    uint64_t multiplier;
    uint_fast8_t type;
    static const size_t widths[8] = { 1u, 2u, 4u, 8u, 1u, 2u, 4u, 8u };

    assert(ts);
    assert(lus);
    assert(lus->size == 0u);

    if (errorToken)
        *errorToken = NULL;
    if (errorString)
        *errorString = NULL;

    SharemindAssemblerLabelLocations ll;
    SharemindAssemblerLabelLocations_init(&ll);

    SharemindAssemblerLabelSlotsTrie lst;
    SharemindAssemblerLabelSlotsTrie_init(&lst);

    {
        SharemindAssemblerLabelLocation * l = SharemindAssemblerLabelLocations_get_or_insert(&ll, "RODATA", NULL);
        if (unlikely(!l))
            goto assemble_out_of_memory;
        /* l->linkingUnit = SIZE_MAX; */ /* Not used. */
        l->section = -1;
        l->offset = 1u;
        l = SharemindAssemblerLabelLocations_get_or_insert(&ll, "DATA", NULL);
        if (unlikely(!l))
            goto assemble_out_of_memory;
        /* l->linkingUnit = SIZE_MAX; */ /* Not used. */
        l->section = -1;
        l->offset = 2u;
        l = SharemindAssemblerLabelLocations_get_or_insert(&ll, "BSS", NULL);
        if (unlikely(!l))
            goto assemble_out_of_memory;
        /* l->linkingUnit = SIZE_MAX; */ /* Not used. */
        l->section = -1;
        l->offset = 3u;
    }


    lu = SharemindAssemblerLinkingUnits_push(lus);
    if (unlikely(!lu))
        goto assemble_out_of_memory;

    SharemindAssemblerLinkingUnit_init(lu);

    if (unlikely(ts->numTokens <= 0))
        goto assemble_ok;

    t = &ts->array[0u];
    e = &ts->array[ts->numTokens];

assemble_newline:
    switch (t->type) {
        case SHAREMIND_ASSEMBLER_TOKEN_NEWLINE:
            break;
        case SHAREMIND_ASSEMBLER_TOKEN_LABEL:
        {
            char * label = SharemindAssemblerToken_label_to_new_string(t);
            if (unlikely(!label))
                goto assemble_out_of_memory;

            int newValue;
            SharemindAssemblerLabelLocation * l = SharemindAssemblerLabelLocations_get_or_insert(&ll, label, &newValue);
            if (unlikely(!l)) {
                free(label);
                goto assemble_out_of_memory;
            }

            /* Check for duplicate label: */
            if (unlikely(!newValue)) {
                free(label);
                goto assemble_duplicate_label_t;
            }

            l->linkingUnit = lu_index;
            l->section = section_index;
            if (section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND) {
                l->offset = numBindings;
            } else if (section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND) {
                l->offset = numPdBindings;
            } else {
                l->offset = lu->sections[section_index].length;
            }

            /* Fill pending label slots: */
            SharemindAssemblerLabelSlots * slots = SharemindAssemblerLabelSlotsTrie_find(&lst, label);
            free(label);
            if (slots) {
                if (!SharemindAssemblerLabelSlots_foreach_with_labelLocationPointer(slots, &SharemindAssemblerLabelSlot_fill, l))
                    goto assemble_invalid_label_t;
            }
            break;
        }
        case SHAREMIND_ASSEMBLER_TOKEN_DIRECTIVE:
            if (t->length == 13u && strncmp(t->text, ".linking_unit", 13u) == 0) {
                SHAREMIND_ASSEMBLER_ASSEMBLE_INC_CHECK_EOF(assemble_unexpected_eof);
                if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_UHEX))
                    goto assemble_invalid_parameter_t;

                const uint64_t v = SharemindAssemblerToken_uhex_value(t);
                if (unlikely(v > UINT8_MAX))
                    goto assemble_invalid_parameter_t;

                if (likely(v != lu_index)) {
                    if (unlikely(v > lus->size))
                        goto assemble_invalid_parameter_t;
                    if (v == lus->size) {
                        lu = SharemindAssemblerLinkingUnits_push(lus);
                        if (unlikely(!lu))
                            goto assemble_out_of_memory;
                        SharemindAssemblerLinkingUnit_init(lu);
                    } else {
                        lu = SharemindAssemblerLinkingUnits_get_pointer(lus, v);
                    }
                    lu_index = v;
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
                }
            } else if (t->length == 8u && strncmp(t->text, ".section", 8u) == 0) {
                SHAREMIND_ASSEMBLER_ASSEMBLE_INC_CHECK_EOF(assemble_unexpected_eof);
                if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_KEYWORD))
                    goto assemble_invalid_parameter_t;

                if (t->length == 4u && strncmp(t->text, "TEXT", 4u) == 0) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
                } else if (t->length == 6u && strncmp(t->text, "RODATA", 6u) == 0) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_RODATA;
                } else if (t->length == 4u && strncmp(t->text, "DATA", 4u) == 0) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_DATA;
                } else if (t->length == 3u && strncmp(t->text, "BSS", 3u) == 0) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS;
                } else if (t->length == 4u && strncmp(t->text, "BIND", 4u) == 0) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND;
                } else if (t->length == 6u && strncmp(t->text, "PDBIND", 6u) == 0) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND;
                } else if (t->length == 5u && strncmp(t->text, "DEBUG", 5u) == 0) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_DEBUG;
                } else {
                    goto assemble_invalid_parameter_t;
                }
            } else if (t->length == 5u && strncmp(t->text, ".data", 5u) == 0) {
                if (unlikely(section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT))
                    goto assemble_unexpected_token_t;

                multiplier = 1u;
                goto assemble_data_or_fill;
            } else if (t->length == 5u && strncmp(t->text, ".fill", 5u) == 0) {
                if (unlikely(section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT
                             || section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND
                             || section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND))
                    goto assemble_unexpected_token_t;

                SHAREMIND_ASSEMBLER_ASSEMBLE_INC_CHECK_EOF(assemble_unexpected_eof);

                if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_UHEX))
                    goto assemble_invalid_parameter_t;

                multiplier = SharemindAssemblerToken_uhex_value(t);
                if (unlikely(multiplier >= 65536u))
                    goto assemble_invalid_parameter_t;

                goto assemble_data_or_fill;
            } else if (likely(t->length == 5u && strncmp(t->text, ".bind", 5u) == 0)) {
                if (unlikely(section_index != SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND && section_index != SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND))
                    goto assemble_unexpected_token_t;

                SHAREMIND_ASSEMBLER_ASSEMBLE_INC_CHECK_EOF(assemble_unexpected_eof);

                if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_STRING))
                    goto assemble_invalid_parameter_t;

                size_t syscallSigLen;
                char * syscallSig = SharemindAssemblerToken_string_value(t, &syscallSigLen);
                if (!syscallSig)
                    goto assemble_out_of_memory;

                const size_t oldLen = lu->sections[section_index].length;
                const size_t newLen = oldLen + syscallSigLen + 1;
                void * newData = realloc(lu->sections[section_index].data, newLen);
                if (unlikely(!newData)) {
                    free(syscallSig);
                    goto assemble_out_of_memory;
                }
                lu->sections[section_index].data = newData;
                lu->sections[section_index].length = newLen;

                memcpy(((uint8_t *) lu->sections[section_index].data) + oldLen,
                       syscallSig, syscallSigLen + 1);

                if (section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND) {
                    numBindings++;
                } else {
                    numPdBindings++;
                }

                free(syscallSig);
            } else {
                goto assemble_unknown_directive_t;
            }

            SHAREMIND_ASSEMBLER_ASSEMBLE_INC_DO_EOL(assemble_check_labels,assemble_unexpected_token_t);
            goto assemble_newline;
        case SHAREMIND_ASSEMBLER_TOKEN_KEYWORD:
        {
            if (unlikely(section_index != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT))
                goto assemble_unexpected_token_t;

            size_t args = 0u;
            size_t l = t->length;
            char * name = (char *) malloc(sizeof(char) * (l + 1u));
            if (unlikely(!name))
                goto assemble_out_of_memory;
            strncpy(name, t->text, l);

            const SharemindAssemblerToken * ot = t;
            /* Collect instruction name and count arguments: */
            for (;;) {
                if (SHAREMIND_ASSEMBLER_ASSEMBLE_INC_EOF_TEST)
                    break;
                if (t->type == SHAREMIND_ASSEMBLER_TOKEN_NEWLINE) {
                    break;
                } else if (t->type == SHAREMIND_ASSEMBLER_TOKEN_KEYWORD) {
                    size_t newSize = l + t->length + 1u;
                    if (unlikely(newSize < l))
                        goto assemble_invalid_parameter_t;
                    char * newName = (char *) realloc(name, sizeof(char) * (newSize + 1u));
                    if (unlikely(!newName)) {
                        free(name);
                        goto assemble_out_of_memory;
                    }
                    name = newName;
                    name[l] = '_';
                    strncpy(name + l + 1u, t->text, t->length);
                    l = newSize;
                } else if (likely(t->type == SHAREMIND_ASSEMBLER_TOKEN_UHEX || t->type == SHAREMIND_ASSEMBLER_TOKEN_HEX || t->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL || t->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL_O)) {
                    args++;
                } else {
                    goto assemble_invalid_parameter_t;
                }
            }
            name[l] = '\0';

            /* Detect and check instruction: */
            const SharemindVmInstruction * i = sharemind_vm_instruction_from_name(name);
            if (unlikely(!i)) {
                if (errorToken)
                    *errorToken = ot;
                if (errorString)
                    *errorString = name;
                goto assemble_unknown_instruction;
            }
            if (unlikely(i->numArgs != args)) {
                if (errorToken)
                    *errorToken = ot;
                if (errorString)
                    *errorString = name;
                goto assemble_invalid_number_of_parameters;
            }
            free(name);

            /* Detect offset for jump instructions */
            size_t jmpOffset;
            int doJumpLabel;
            {
                union { uint64_t code; char c[8]; } x;
                x.code = i->code;
                if (x.c[0] == 0x04     /* Check for jump namespace */
                    && x.c[2] == 0x01) /* and imm first argument OLB */
                {
                    jmpOffset = lu->sections[section_index].length;
                    doJumpLabel = 1;
                } else {
                    jmpOffset = 0u;
                    doJumpLabel = 0;
                }
            }

            /* Allocate whole instruction: */
            char * newData = (char *) realloc((void *) lu->sections[section_index].data, sizeof(SharemindCodeBlock) * (lu->sections[section_index].length + args + 1));
            if (unlikely(!newData))
                goto assemble_out_of_memory;
            lu->sections[section_index].data = newData;
            SharemindCodeBlock * cbdata = (SharemindCodeBlock *) lu->sections[section_index].data;
            SharemindCodeBlock * instr = &cbdata[lu->sections[section_index].length];
            lu->sections[section_index].length += args + 1;

            /* Write instruction code */
            instr->uint64[0] = i->code;

            /* Write arguments: */
            for (;;) {
                if (++ot == t)
                    break;
                if (ot->type == SHAREMIND_ASSEMBLER_TOKEN_UHEX) {
                    doJumpLabel = 0; /* Past first argument */
                    instr++;
                    instr->uint64[0] = SharemindAssemblerToken_uhex_value(ot);
                } else if (ot->type == SHAREMIND_ASSEMBLER_TOKEN_HEX) {
                    doJumpLabel = 0; /* Past first argument */
                    instr++;
                    instr->int64[0] = SharemindAssemblerToken_hex_value(ot);
                } else if (likely(ot->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL || ot->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL_O)) {
                    instr++;
                    char * label = SharemindAssemblerToken_label_to_new_string(ot);
                    if (unlikely(!label))
                        goto assemble_out_of_memory;

                    /* Check whether label is defined: */
                    SharemindAssemblerLabelLocation * loc = SharemindAssemblerLabelLocations_find(&ll, label);
                    if (loc) {
                        free(label);

                        /* Is this a jump instruction location? */
                        if (doJumpLabel) {
                            if (loc->section < 0)
                                goto assemble_invalid_label;

                            assert(jmpOffset >= loc->offset); /* Because we're one-pass. */

                            /* Check whether the label is defined in the same linking unit: */
                            if (loc->linkingUnit != lu_index) {
                                if (errorToken)
                                    *errorToken = ot;
                                goto assemble_invalid_label;
                            }

                            /* Verify that the label is defined in a TEXT section: */
                            assert(section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT);
                            if (loc->section != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT) {
                                if (errorToken)
                                    *errorToken = ot;
                                goto assemble_invalid_label;
                            }

                            size_t absTarget = loc->offset;
                            if (!sharemind_assembler_assign_add_sizet_int64(&absTarget, SharemindAssemblerToken_label_offset(ot))
                                || !sharemind_assembler_substract_sizet_sizet_to_int64(&instr->int64[0], absTarget, jmpOffset))
                            {
                                if (errorToken)
                                    *errorToken = ot;
                                goto assemble_invalid_label_offset;
                            }
                            /** \todo Maybe check whether there's really an instruction there */
                        } else {
                            size_t absTarget = loc->offset;
                            int64_t offset = SharemindAssemblerToken_label_offset(ot);
                            if (loc->section < 0) {
                                if (offset != 0)
                                    goto assemble_invalid_label_offset;
                            } else {
                                if (!sharemind_assembler_assign_add_sizet_int64(&absTarget, offset)) {
                                    if (errorToken)
                                        *errorToken = ot;
                                    goto assemble_invalid_label_offset;
                                }
                            }
                            instr->uint64[0] = absTarget;
                        }
                    } else {
                        int newValue;
                        SharemindAssemblerLabelSlots * slots = SharemindAssemblerLabelSlotsTrie_get_or_insert(&lst, label, &newValue);
                        free(label);
                        if (unlikely(!slots))
                            goto assemble_out_of_memory;

                        if (newValue)
                            SharemindAssemblerLabelSlots_init(slots);

                        SharemindAssemblerLabelSlot * slot = SharemindAssemblerLabelSlots_push(slots);
                        if (unlikely(!slot))
                            goto assemble_out_of_memory;

                        slot->linkingUnit = lu_index;
                        slot->section = section_index;
                        slot->extraOffset = SharemindAssemblerToken_label_offset(ot);
                        slot->doJumpLabel = doJumpLabel; /* Signal a relative jump label */
                        slot->jmpOffset = jmpOffset;
                        slot->data = &lu->sections[section_index].data;
                        assert(instr > (SharemindCodeBlock *) lu->sections[section_index].data);
                        assert(((uintmax_t) (instr - (SharemindCodeBlock *) lu->sections[section_index].data)) <= SIZE_MAX);
                        slot->cbdata_index = (size_t) (instr - (SharemindCodeBlock *) lu->sections[section_index].data);
                        slot->token = ot;
                    }
                    doJumpLabel = 0; /* Past first argument */
                } else {
                    /* Skip keywords, because they're already included in the instruction code. */
                    assert(ot->type == SHAREMIND_ASSEMBLER_TOKEN_KEYWORD);
                }
            }

            SHAREMIND_ASSEMBLER_ASSEMBLE_DO_EOL(assemble_check_labels,assemble_unexpected_token_t);
            goto assemble_newline;
        }
        default:
            goto assemble_unexpected_token_t;
    } /* switch */

    if (!SHAREMIND_ASSEMBLER_ASSEMBLE_INC_EOF_TEST)
        goto assemble_newline;

assemble_check_labels:

    /* Check for undefined labels: */
    {
        SharemindAssemblerLabelSlot * undefinedSlot;
        if (likely(SharemindAssemblerLabelSlotsTrie_foreach_with_labelSlotPointerPointer(&lst, &SharemindAssemblerLabelSlots_all_slots_filled, &undefinedSlot)))
            goto assemble_ok;

        assert(undefinedSlot);
        if (errorToken)
            *errorToken = undefinedSlot->token;
        goto assemble_undefined_label;
    }

assemble_data_or_fill:

    SHAREMIND_ASSEMBLER_ASSEMBLE_INC_CHECK_EOF(assemble_unexpected_eof);

    if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_KEYWORD))
        goto assemble_invalid_parameter_t;

    if (t->length == 5u && strncmp(t->text, "uint8", t->length) == 0) {
        type = 0u;
    } else if (t->length == 6u && strncmp(t->text, "uint16", t->length) == 0) {
        type = 1u;
    } else if (t->length == 6u && strncmp(t->text, "uint32", t->length) == 0) {
        type = 2u;
    } else if (t->length == 6u && strncmp(t->text, "uint64", t->length) == 0) {
        type = 3u;
    } else if (t->length == 4u && strncmp(t->text, "int8", t->length) == 0) {
        type = 4u;
    } else if (t->length == 5u && strncmp(t->text, "int16", t->length) == 0) {
        type = 5u;
    } else if (t->length == 5u && strncmp(t->text, "int32", t->length) == 0) {
        type = 6u;
    } else if (t->length == 5u && strncmp(t->text, "int64", t->length) == 0) {
        type = 7u;
    } else if (t->length == 6u && strncmp(t->text, "string", t->length) == 0) {
        type = 8u;
    } else {
        goto assemble_invalid_parameter_t;
    }

    if (type < 8u) {
        dataToWriteLength = widths[type];
    } else {
        assert(type == 8u);
        dataToWriteLength = 0u;
    }

    SHAREMIND_ASSEMBLER_ASSEMBLE_INC_DO_EOL(assemble_data_write,assemble_data_opt_param);
    goto assemble_data_write;

assemble_data_opt_param:

    assert(!dataToWrite);
    if (t->type == SHAREMIND_ASSEMBLER_TOKEN_UHEX) {
        const uint64_t v = SharemindAssemblerToken_uhex_value(t);
        switch (type) {
            case 0u: /* uint8 */
                if (v > UINT8_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 1u: /* uint16 */
                if (v > UINT16_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 2u: /* uint32 */
                if (v > UINT32_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 3u: /* uint64; All should be fine here. */
                break;
            case 4u: /* int8 */
                if (v > INT8_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 5u: /* int16 */
                if (v > INT16_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 6u: /* int32 */
                if (v > INT32_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 7u: /* int64 */
                if (v > INT64_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 8u: /* string */
                goto assemble_invalid_parameter_t;
            default:
                abort();
        }
        dataToWrite = malloc(dataToWriteLength);
        if (!dataToWrite)
            goto assemble_out_of_memory;
        memcpy(dataToWrite, &v, dataToWriteLength);
    } else if (t->type == SHAREMIND_ASSEMBLER_TOKEN_HEX) {
        const int64_t v = SharemindAssemblerToken_hex_value(t);
        switch (type) {
            case 0u: /* uint8 */
                if (v > UINT8_MAX || v < 0)
                    goto assemble_invalid_parameter_t;
                break;
            case 1u: /* uint16 */
                if (v > UINT16_MAX || v < 0)
                    goto assemble_invalid_parameter_t;
                break;
            case 2u: /* uint32 */
                if (v > UINT32_MAX || v < 0)
                    goto assemble_invalid_parameter_t;
                break;
            case 3u: /* uint64 */
                if (v < 0)
                    goto assemble_invalid_parameter_t;
                break;
            case 4u: /* int8 */
                if (v < INT8_MIN || v > INT8_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 5u: /* int16 */
                if (v < INT16_MIN || v > INT16_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 6u: /* int32 */
                if (v < INT32_MIN || v > INT32_MAX)
                    goto assemble_invalid_parameter_t;
                break;
            case 7u: /* int64; All should be fine here. */
                break;
            case 8u: /* string */
                goto assemble_invalid_parameter_t;
            default:
                abort();
        }
        dataToWrite = malloc(dataToWriteLength);
        if (!dataToWrite)
            goto assemble_out_of_memory;
        memcpy(dataToWrite, &v, dataToWriteLength);
    } else if (t->type == SHAREMIND_ASSEMBLER_TOKEN_STRING && type == 8u) {
        dataToWrite = SharemindAssemblerToken_string_value(t, &dataToWriteLength);
        if (!dataToWrite)
            goto assemble_out_of_memory;
    } else {
        goto assemble_invalid_parameter_t;
    }
    assert(dataToWrite);

    SHAREMIND_ASSEMBLER_ASSEMBLE_INC_DO_EOL(assemble_data_write,assemble_unexpected_token_t);

assemble_data_write:

    assert(section_index != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT);
    if (section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
        lu->sections[SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS].length += (multiplier * dataToWriteLength);
    } else {
        const size_t oldLen = lu->sections[section_index].length;
        const size_t newLen = oldLen + (multiplier * dataToWriteLength);
        void * newData = realloc(lu->sections[section_index].data, newLen);
        if (unlikely(!newData)) {
            free(dataToWrite);
            goto assemble_out_of_memory;
        }
        lu->sections[section_index].data = newData;
        lu->sections[section_index].length = newLen;

        /* Actually write the values. */
        newData = ((uint8_t *) newData) + oldLen;
        if (dataToWrite) {
            for (;;) {
                memcpy(newData, dataToWrite, dataToWriteLength);
                if (!--multiplier)
                    break;
                newData = ((uint8_t *) newData) + dataToWriteLength;
            };
        } else {
            memset(newData, 0, dataToWriteLength);
        }
    }

    if (dataToWrite)
        free(dataToWrite);
    dataToWrite = NULL;
    goto assemble_newline;

assemble_ok:
    returnStatus = SHAREMIND_ASSEMBLE_OK;
    goto assemble_free_and_return;

assemble_out_of_memory:
    returnStatus = SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
    goto assemble_free_and_return;

assemble_unexpected_token_t:
    if (errorToken)
        *errorToken = t;
    if (errorString) {
        *errorString = (char *) malloc(t->length + 1);
        strncpy(*errorString, t->text, t->length);
        *errorString[t->length] = '\0';
    }
    returnStatus = SHAREMIND_ASSEMBLE_UNEXPECTED_TOKEN;
    goto assemble_free_and_return;

assemble_unexpected_eof:
    returnStatus = SHAREMIND_ASSEMBLE_UNEXPECTED_EOF;
    goto assemble_free_and_return;

assemble_duplicate_label_t:
    if (errorToken)
        *errorToken = t;
    returnStatus = SHAREMIND_ASSEMBLE_DUPLICATE_LABEL;
    goto assemble_free_and_return;

assemble_unknown_directive_t:
    if (errorToken)
        *errorToken = t;
    returnStatus = SHAREMIND_ASSEMBLE_UNKNOWN_DIRECTIVE;
    goto assemble_free_and_return;

assemble_unknown_instruction:
    returnStatus = SHAREMIND_ASSEMBLE_UNKNOWN_INSTRUCTION;
    goto assemble_free_and_return;

assemble_invalid_number_of_parameters:
    returnStatus = SHAREMIND_ASSEMBLE_INVALID_NUMBER_OF_PARAMETERS;
    goto assemble_free_and_return;

assemble_invalid_parameter_t:
    if (errorToken)
        *errorToken = t;
    returnStatus = SHAREMIND_ASSEMBLE_INVALID_PARAMETER;
    goto assemble_free_and_return;

assemble_undefined_label:
    returnStatus = SHAREMIND_ASSEMBLE_UNDEFINED_LABEL;
    goto assemble_free_and_return;

assemble_invalid_label_t:
    if (errorToken)
        *errorToken = t;
assemble_invalid_label:
    returnStatus = SHAREMIND_ASSEMBLE_INVALID_LABEL;
    goto assemble_free_and_return;

assemble_invalid_label_offset:
    returnStatus = SHAREMIND_ASSEMBLE_INVALID_LABEL_OFFSET;
    goto assemble_free_and_return;

assemble_free_and_return:
    SharemindAssemblerLabelSlotsTrie_destroy_with(&lst, &SharemindAssemblerLabelSlots_destroy);
    SharemindAssemblerLabelLocations_destroy(&ll);
    return returnStatus;
}
