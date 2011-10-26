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
#include <stdio.h>
#include <stdlib.h>
#include "../libsmvmi/instr.h"
#include "../likely.h"
#include "../trie.h"
#include "tokens.h"


static inline int SMAS_Assemble_assign_add_sizet_int64(size_t * lhs, const int64_t rhs) {
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

static inline int SMAS_Assemble_substract_sizet_sizet_to_int64(int64_t *dest, const size_t lhs, const size_t rhs) {
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


struct SMAS_LabelLocation {
    size_t linkingUnit;
    int section;
    size_t offset;
};

SM_TRIE_DECLARE(SMAS_LabelLocations,struct SMAS_LabelLocation,)
SM_TRIE_DEFINE(SMAS_LabelLocations,struct SMAS_LabelLocation,malloc,free,)

struct SMAS_LabelSlot {
    size_t linkingUnit;
    int section;
    int64_t extraOffset;
    int doJumpLabel;
    size_t jmpOffset;
    union SM_CodeBlock ** cbdata;
    size_t cbdata_index;
    const struct SMAS_Token * token; /* NULL if this slot is already filled */
};

static int SMAS_LabelSlot_filled(struct SMAS_LabelSlot * s, struct SMAS_LabelSlot ** d) {
    assert(s);
    if (s->token == NULL)
        return 1;
    (*d) = s;
    return 0;
}

SM_VECTOR_DECLARE(SMAS_LabelSlots,struct SMAS_LabelSlot,,)
SM_VECTOR_DEFINE(SMAS_LabelSlots,struct SMAS_LabelSlot,malloc,free,realloc,)
SM_VECTOR_DECLARE_FOREACH_WITH(SMAS_LabelSlots,struct SMAS_LabelSlot,labelLocationPointer,struct SMAS_LabelLocation *,struct SMAS_LabelLocation * p,)
SM_VECTOR_DEFINE_FOREACH_WITH(SMAS_LabelSlots,struct SMAS_LabelSlot,labelLocationPointer,struct SMAS_LabelLocation *,struct SMAS_LabelLocation * p,p,)
SM_VECTOR_DECLARE_FOREACH_WITH(SMAS_LabelSlots,struct SMAS_LabelSlot,labelSlotPointerPointer,struct SMAS_LabelSlot **,struct SMAS_LabelSlot ** p,)
SM_VECTOR_DEFINE_FOREACH_WITH(SMAS_LabelSlots,struct SMAS_LabelSlot,labelSlotPointerPointer,struct SMAS_LabelSlot **,struct SMAS_LabelSlot ** p,p,)

static int SMAS_LabelSlots_allSlotsFilled(struct SMAS_LabelSlots * ss, struct SMAS_LabelSlot ** d) {
    return SMAS_LabelSlots_foreach_with_labelSlotPointerPointer(ss, &SMAS_LabelSlot_filled, d);
}

SM_TRIE_DECLARE(SMAS_LabelSlotsTrie,struct SMAS_LabelSlots,)
SM_TRIE_DEFINE(SMAS_LabelSlotsTrie,struct SMAS_LabelSlots,malloc,free,)
SM_TRIE_DECLARE_FOREACH_WITH(SMAS_LabelSlotsTrie,struct SMAS_LabelSlots,labelSlotPointerPointer,struct SMAS_LabelSlot **,struct SMAS_LabelSlot ** p,)
SM_TRIE_DEFINE_FOREACH_WITH(SMAS_LabelSlotsTrie,struct SMAS_LabelSlots,labelSlotPointerPointer,struct SMAS_LabelSlot **,struct SMAS_LabelSlot ** p,p,)

static int SMAS_LabelSlot_fill(struct SMAS_LabelSlot * s, struct SMAS_LabelLocation * l) {
    assert(s);
    assert(s->token);
    assert(l);

    size_t absTarget = l->offset;
    if (!SMAS_Assemble_assign_add_sizet_int64(&absTarget, s->extraOffset))
        goto SMAS_LabelSlot_fill_error;

    if (!s->doJumpLabel) { /* Normal absolute label */
        (*s->cbdata)[s->cbdata_index].uint64[0] = absTarget;
    } else { /* Relative jump label */
        if (s->linkingUnit != l->linkingUnit || s->section != l->section)
            goto SMAS_LabelSlot_fill_error;

        assert(s->section == SME_SECTION_TYPE_TEXT);
        assert(s->jmpOffset < l->offset); /* Because we're one-pass. */

        if (!SMAS_Assemble_substract_sizet_sizet_to_int64(&(*s->cbdata)[s->cbdata_index].int64[0], absTarget, s->jmpOffset))
            goto SMAS_LabelSlot_fill_error;

        /** \todo Maybe check whether there's really an instruction there */
    }
    s->token = NULL;
    return 1;

SMAS_LabelSlot_fill_error:
    /** \todo Provide better diagnostics */
    return 0;
}

SM_ENUM_CUSTOM_DEFINE_TOSTRING(SMAS_Assemble_Error, SMAS_ENUM_Assemble_Error);

#define SMAS_ASSEMBLE_EOF_TEST     (unlikely(  t >= e))
#define SMAS_ASSEMBLE_INC_EOF_TEST (unlikely(++t >= e))

#define SMAS_ASSEMBLE_INC_CHECK_EOF(eof) \
    if (SMAS_ASSEMBLE_INC_EOF_TEST) { \
        goto eof; \
    } else (void) 0

#define SMAS_ASSEMBLE_DO_EOL(eof,noexpect) \
    if (1) { \
        if (SMAS_ASSEMBLE_EOF_TEST) \
            goto eof; \
        if (unlikely(t->type != SMAS_TOKEN_NEWLINE)) \
            goto noexpect; \
        goto smas_assemble_newline; \
    } else (void) 0

#define SMAS_ASSEMBLE_INC_DO_EOL(eof,noexpect) \
    if (1) { \
        t++; \
        SMAS_ASSEMBLE_DO_EOL(eof,noexpect); \
    } else (void) 0

enum SMAS_Assemble_Error SMAS_assemble(const struct SMAS_Tokens * ts,
                                       struct SMAS_LinkingUnits * lus,
                                       const struct SMAS_Token ** errorToken,
                                       char ** errorString)
{
    assert(ts);
    assert(lus);
    assert(lus->size == 0u);

    int returnStatus;
    *errorToken = NULL;
    *errorString = NULL;

    struct SMAS_LabelLocations ll;
    SMAS_LabelLocations_init(&ll);

    struct SMAS_LabelSlotsTrie lst;
    SMAS_LabelSlotsTrie_init(&lst);

    struct SMAS_LinkingUnit * lu = SMAS_LinkingUnits_push(lus);
    if (unlikely(!lu))
        goto smas_assemble_out_of_memory;

    SMAS_LinkingUnit_init(lu);

    if (unlikely(ts->numTokens <= 0))
        goto smas_assemble_ok;

    struct SMAS_Token * t = &ts->array[0u];
    struct SMAS_Token * e = &ts->array[ts->numTokens];

    size_t lu_index = 0u;
    int section_index = SME_SECTION_TYPE_TEXT;

    /* for .data and .fill: */
    uint64_t multiplier;
    uint_fast8_t type;
    static const size_t widths[8] = { 1u, 2u, 4u, 8u, 1u, 2u, 4u, 8u };

smas_assemble_newline:
    switch (t->type) {
        case SMAS_TOKEN_NEWLINE:
            break;
        case SMAS_TOKEN_LABEL:
        {
            char * label = SMAS_token_label_label_new(t);
            if (unlikely(!label))
                goto smas_assemble_out_of_memory;

            int newValue;
            struct SMAS_LabelLocation * l = SMAS_LabelLocations_get_or_insert(&ll, label, &newValue);
            if (unlikely(!l)) {
                free(label);
                goto smas_assemble_out_of_memory;
            }

            /* Check for duplicate label: */
            if (unlikely(!newValue)) {
                free(label);
                goto smas_assemble_duplicate_label_t;
            }

            l->linkingUnit = lu_index;
            l->section = section_index;
            l->offset = lu->sections[section_index].length;

            /* Fill pending label slots: */
            struct SMAS_LabelSlots * slots = SMAS_LabelSlotsTrie_find(&lst, label);
            free(label);
            if (slots) {
                if (!SMAS_LabelSlots_foreach_with_labelLocationPointer(slots, &SMAS_LabelSlot_fill, l))
                    goto smas_assemble_invalid_label_t;
            }
            break;
        }
        case SMAS_TOKEN_DIRECTIVE:
            if (t->length == 13u && strncmp(t->text, ".linking_unit", t->length) == 0) {
                SMAS_ASSEMBLE_INC_CHECK_EOF(smas_assemble_unexpected_eof);
                if (unlikely(t->type != SMAS_TOKEN_UHEX))
                    goto smas_assemble_invalid_parameter_t;

                const uint64_t v = SMAS_token_uhex_value(t);
                if (unlikely(v > UINT8_MAX))
                    goto smas_assemble_invalid_parameter_t;

                if (likely(v != lu_index)) {
                    if (unlikely(v > lus->size))
                        goto smas_assemble_invalid_parameter_t;
                    if (v == lus->size) {
                        lu = SMAS_LinkingUnits_push(lus);
                        if (unlikely(!lu))
                            goto smas_assemble_out_of_memory;
                    } else {
                        lu = SMAS_LinkingUnits_get_pointer(lus, v);
                    }
                    lu_index = v;
                    section_index = SME_SECTION_TYPE_TEXT;
                }

                SMAS_ASSEMBLE_INC_DO_EOL(smas_assemble_check_labels,smas_assemble_unexpected_token_t);
            } else if (t->length == 8u && strncmp(t->text, ".section", t->length) == 0) {
                SMAS_ASSEMBLE_INC_CHECK_EOF(smas_assemble_unexpected_eof);
                if (unlikely(t->type != SMAS_TOKEN_KEYWORD))
                    goto smas_assemble_invalid_parameter_t;

                if (t->length == 4u && strncmp(t->text, "TEXT", t->length) == 0) {
                    section_index = SME_SECTION_TYPE_TEXT;
                } else if (t->length == 6u && strncmp(t->text, "RODATA", t->length) == 0) {
                    section_index = SME_SECTION_TYPE_RODATA;
                } else if (t->length == 4u && strncmp(t->text, "DATA", t->length) == 0) {
                    section_index = SME_SECTION_TYPE_DATA;
                } else if (t->length == 3u && strncmp(t->text, "BSS", t->length) == 0) {
                    section_index = SME_SECTION_TYPE_BSS;
                } else if (t->length == 4u && strncmp(t->text, "BIND", t->length) == 0) {
                    section_index = SME_SECTION_TYPE_BIND;
                } else if (t->length == 5u && strncmp(t->text, "DEBUG", t->length) == 0) {
                    section_index = SME_SECTION_TYPE_DEBUG;
                } else {
                    goto smas_assemble_invalid_parameter_t;
                }
            } else if (t->length == 5u && strncmp(t->text, ".data", t->length) == 0) {
                if (unlikely(section_index == SME_SECTION_TYPE_TEXT))
                    goto smas_assemble_unexpected_token_t;

                multiplier = 1u;
                goto smas_assemble_data_or_fill;
            } else if (t->length == 5u && strncmp(t->text, ".fill", t->length) == 0) {
                if (unlikely(section_index == SME_SECTION_TYPE_TEXT || section_index == SME_SECTION_TYPE_BIND))
                    goto smas_assemble_unexpected_token_t;

                SMAS_ASSEMBLE_INC_CHECK_EOF(smas_assemble_unexpected_eof);

                if (unlikely(t->type != SMAS_TOKEN_UHEX))
                    goto smas_assemble_invalid_parameter_t;

                multiplier = SMAS_token_uhex_value(t);
                if (unlikely(multiplier >= 65536u))
                    goto smas_assemble_invalid_parameter_t;

                goto smas_assemble_data_or_fill;
            } else if (likely(t->length == 13u && strncmp(t->text, ".bind_syscall", t->length) == 0)) {
                if (unlikely(section_index != SME_SECTION_TYPE_BIND))
                    goto smas_assemble_unexpected_token_t;

                fprintf(stderr, "TODO\n");
                goto smas_assemble_unknown_directive_t;
            } else {
                goto smas_assemble_unknown_directive_t;
            }
            break;
        case SMAS_TOKEN_KEYWORD:
        {
            if (unlikely(section_index != SME_SECTION_TYPE_TEXT))
                goto smas_assemble_unexpected_token_t;

            size_t args = 0u;
            size_t l = t->length;
            char * name = (char *) malloc(sizeof(char) * (l + 1u));
            if (unlikely(!name))
                goto smas_assemble_out_of_memory;
            strncpy(name, t->text, l);

            const struct SMAS_Token * ot = t;
            /* Collect instruction name and count arguments: */
            for (;;) {
                if (SMAS_ASSEMBLE_INC_EOF_TEST)
                    break;
                if (t->type == SMAS_TOKEN_NEWLINE) {
                    break;
                } else if (t->type == SMAS_TOKEN_KEYWORD) {
                    size_t newSize = l + t->length + 1u;
                    if (unlikely(newSize < l))
                        goto smas_assemble_invalid_parameter_t;
                    char * newName = (char *) realloc(name, sizeof(char) * (newSize + 1u));
                    if (unlikely(!newName)) {
                        free(name);
                        goto smas_assemble_out_of_memory;
                    }
                    name = newName;
                    name[l] = '_';
                    strncpy(name + l + 1u, t->text, t->length);
                    l = newSize;
                } else if (likely(t->type == SMAS_TOKEN_UHEX || t->type == SMAS_TOKEN_HEX || t->type == SMAS_TOKEN_LABEL || t->type == SMAS_TOKEN_LABEL_O)) {
                    args++;
                } else {
                    goto smas_assemble_invalid_parameter_t;
                }
            }
            name[l] = '\0';

            /* Detect and check instruction: */
            const struct SMVMI_Instruction * i = SMVMI_Instruction_from_name(name);
            if (unlikely(!i)) {
                *errorToken = ot;
                *errorString = name;
                goto smas_assemble_unknown_instruction;
            }
            if (unlikely(i->numargs != args)) {
                *errorToken = ot;
                *errorString = name;
                goto smas_assemble_invalid_number_of_parameters;
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
            char * newData = (char *) realloc((void *) lu->sections[section_index].data, sizeof(union SM_CodeBlock) * (lu->sections[section_index].length + args + 1));
            if (unlikely(!newData))
                goto smas_assemble_out_of_memory;
            lu->sections[section_index].data = newData;
            union SM_CodeBlock * instr = &lu->sections[section_index].cbdata[lu->sections[section_index].length];
            lu->sections[section_index].length += args + 1;

            /* Write instruction code */
            instr->uint64[0] = i->code;

            /* Write arguments: */
            for (;;) {
                if (++ot == t)
                    break;
                if (ot->type == SMAS_TOKEN_UHEX) {
                    doJumpLabel = 0; /* Past first argument */
                    instr++;
                    instr->uint64[0] = SMAS_token_uhex_value(ot);
                } else if (ot->type == SMAS_TOKEN_HEX) {
                    doJumpLabel = 0; /* Past first argument */
                    instr++;
                    instr->int64[0] = SMAS_token_hex_value(ot);
                } else if (likely(ot->type == SMAS_TOKEN_LABEL || ot->type == SMAS_TOKEN_LABEL_O)) {
                    instr++;
                    char * label = SMAS_token_label_label_new(ot);
                    if (unlikely(!label))
                        goto smas_assemble_out_of_memory;

                    /* Check whether label is defined: */
                    struct SMAS_LabelLocation * loc = SMAS_LabelLocations_find(&ll, label);
                    if (loc) {
                        free(label);

                        /* Is this a jump instruction location? */
                        if (doJumpLabel) {
                            assert(jmpOffset >= loc->offset); /* Because we're one-pass. */

                            /* Check whether the label is defined in the same linking unit: */
                            if (loc->linkingUnit != lu_index) {
                                *errorToken = ot;
                                goto smas_assemble_invalid_label;
                            }

                            /* Verify that the label is defined in a TEXT section: */
                            assert(section_index == SME_SECTION_TYPE_TEXT);
                            if (loc->section != SME_SECTION_TYPE_TEXT) {
                                *errorToken = ot;
                                goto smas_assemble_invalid_label;
                            }

                            size_t absTarget = loc->offset;
                            if (!SMAS_Assemble_assign_add_sizet_int64(&absTarget, SMAS_token_label_offset(ot))
                                || !SMAS_Assemble_substract_sizet_sizet_to_int64(&instr->int64[0], absTarget, jmpOffset))
                            {
                                *errorToken = ot;
                                goto smas_assemble_invalid_label_offset;
                            }
                            /** \todo Maybe check whether there's really an instruction there */
                        } else {
                            size_t absTarget = loc->offset;
                            if (!SMAS_Assemble_assign_add_sizet_int64(&absTarget, SMAS_token_label_offset(ot))) {
                                *errorToken = ot;
                                goto smas_assemble_invalid_label_offset;
                            }
                            instr->uint64[0] = absTarget;
                        }
                    } else {
                        int newValue;
                        struct SMAS_LabelSlots * slots = SMAS_LabelSlotsTrie_get_or_insert(&lst, label, &newValue);
                        free(label);
                        if (unlikely(!slots))
                            goto smas_assemble_out_of_memory;

                        if (newValue)
                            SMAS_LabelSlots_init(slots);

                        struct SMAS_LabelSlot * slot = SMAS_LabelSlots_push(slots);
                        if (unlikely(!slot))
                            goto smas_assemble_out_of_memory;

                        slot->linkingUnit = lu_index;
                        slot->section = section_index;
                        slot->extraOffset = SMAS_token_label_offset(ot);
                        slot->doJumpLabel = doJumpLabel; /* Signal a relative jump label */
                        slot->jmpOffset = jmpOffset;
                        slot->cbdata = &lu->sections[section_index].cbdata;
                        assert(instr > lu->sections[section_index].cbdata);
                        assert(((uintmax_t) (instr - lu->sections[section_index].cbdata)) <= SIZE_MAX);
                        slot->cbdata_index = (size_t) (instr - lu->sections[section_index].cbdata);
                        slot->token = ot;
                    }
                    doJumpLabel = 0; /* Past first argument */
                } else {
                    /* Skip keywords, because they're already included in the instruction code. */
                    assert(ot->type == SMAS_TOKEN_KEYWORD);
                }
            }

            SMAS_ASSEMBLE_DO_EOL(smas_assemble_check_labels,smas_assemble_unexpected_token_t);
            abort();
        }
        default:
            goto smas_assemble_unexpected_token_t;
    } /* switch */

    if (!SMAS_ASSEMBLE_INC_EOF_TEST)
        goto smas_assemble_newline;

smas_assemble_check_labels:

    /* Check for undefined labels: */
    {
        struct SMAS_LabelSlot * undefinedSlot;
        if (likely(SMAS_LabelSlotsTrie_foreach_with_labelSlotPointerPointer(&lst, &SMAS_LabelSlots_allSlotsFilled, &undefinedSlot)))
            goto smas_assemble_ok;

        assert(undefinedSlot);
        *errorToken = undefinedSlot->token;
        *errorString = SMAS_token_label_label_new(undefinedSlot->token);
        goto smas_assemble_undefined_label;
    }

smas_assemble_data_or_fill:

    SMAS_ASSEMBLE_INC_CHECK_EOF(smas_assemble_unexpected_eof);

    if (unlikely(t->type != SMAS_TOKEN_KEYWORD))
        goto smas_assemble_invalid_parameter_t;

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
    } else {
        goto smas_assemble_invalid_parameter_t;
    }

    SMAS_ASSEMBLE_INC_DO_EOL(smas_assemble_ok,smas_assemble_data_opt_param);

smas_assemble_data_opt_param:

    if (t->type == SMAS_TOKEN_UHEX) {
        const uint64_t v = SMAS_token_uhex_value(t);
        switch (type) {
            case 0u: /* uint8 */
                if (v > UINT8_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 1u: /* uint16 */
                if (v > UINT16_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 2u: /* uint32 */
                if (v > UINT32_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 3u: /* uint64; All should be fine here. */
                break;
            case 4u: /* int8 */
                if (v > INT8_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 5u: /* int16 */
                if (v > INT16_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 6u: /* int32 */
                if (v > INT32_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 7u: /* int64 */
                if (v > INT64_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            default:
                abort();
        }
    } else if (t->type == SMAS_TOKEN_HEX) {
        const int64_t v = SMAS_token_hex_value(t);
        switch (type) {
            case 0u: /* uint8 */
                if (v > UINT8_MAX || v < 0)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 1u: /* uint16 */
                if (v > UINT16_MAX || v < 0)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 2u: /* uint32 */
                if (v > UINT32_MAX || v < 0)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 3u: /* uint64 */
                if (v < 0)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 4u: /* int8 */
                if (v < INT8_MIN || v > INT8_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 5u: /* int16 */
                if (v < INT16_MIN || v > INT16_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 6u: /* int32 */
                if (v < INT32_MIN || v > INT32_MAX)
                    goto smas_assemble_invalid_parameter_t;
                break;
            case 7u: /* int64; All should be fine here. */
                break;
            default:
                abort();
        }
    } else {
        goto smas_assemble_invalid_parameter_t;
    }

    SMAS_ASSEMBLE_INC_CHECK_EOF(smas_assemble_unexpected_eof);
    lu->sections[section_index].length += (multiplier * widths[type]);
    /** \todo Actually write the values. */
    goto smas_assemble_newline;

smas_assemble_ok:
    returnStatus = SMAS_ASSEMBLE_OK;
    goto smas_assemble_free_and_return;

smas_assemble_out_of_memory:
    returnStatus = SMAS_ASSEMBLE_OUT_OF_MEMORY;
    goto smas_assemble_free_and_return;

smas_assemble_unexpected_token_t:
    *errorToken = t;
    *errorString = (char *) malloc(t->length + 1);
    strncpy(*errorString, t->text, t->length);
    *errorString[t->length] = '\0';
    returnStatus = SMAS_ASSEMBLE_UNEXPECTED_TOKEN;
    goto smas_assemble_free_and_return;

smas_assemble_unexpected_eof:
    returnStatus = SMAS_ASSEMBLE_UNEXPECTED_EOF;
    goto smas_assemble_free_and_return;

smas_assemble_duplicate_label_t:
    *errorToken = t;
    returnStatus = SMAS_ASSEMBLE_DUPLICATE_LABEL;
    goto smas_assemble_free_and_return;

smas_assemble_unknown_directive_t:
    *errorToken = t;
    returnStatus = SMAS_ASSEMBLE_UNKNOWN_DIRECTIVE;
    goto smas_assemble_free_and_return;

smas_assemble_unknown_instruction:
    returnStatus = SMAS_ASSEMBLE_UNKNOWN_INSTRUCTION;
    goto smas_assemble_free_and_return;

smas_assemble_invalid_number_of_parameters:
    returnStatus = SMAS_ASSEMBLE_INVALID_NUMBER_OF_PARAMETERS;
    goto smas_assemble_free_and_return;

smas_assemble_invalid_parameter_t:
    *errorToken = t;
    returnStatus = SMAS_ASSEMBLE_INVALID_PARAMETER;
    goto smas_assemble_free_and_return;

smas_assemble_undefined_label:
    returnStatus = SMAS_ASSEMBLE_UNDEFINED_LABEL;
    goto smas_assemble_free_and_return;

smas_assemble_invalid_label_t:
    *errorToken = t;
smas_assemble_invalid_label:
    returnStatus = SMAS_ASSEMBLE_INVALID_LABEL;
    goto smas_assemble_free_and_return;

smas_assemble_invalid_label_offset:
    returnStatus = SMAS_ASSEMBLE_INVALID_LABEL_OFFSET;
    goto smas_assemble_free_and_return;

smas_assemble_free_and_return:
    SMAS_LabelSlotsTrie_destroy_with(&lst, &SMAS_LabelSlots_destroy);
    SMAS_LabelLocations_destroy(&ll);
    return returnStatus;
}
