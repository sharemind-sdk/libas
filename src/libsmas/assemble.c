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


struct SMAS_LabelLocation {
    size_t linkingUnit;
    int section;
    size_t offset;
};

SM_TRIE_DECLARE(SMAS_LabelLocations,struct SMAS_LabelLocation)
SM_TRIE_DEFINE(SMAS_LabelLocations,struct SMAS_LabelLocation,malloc,free)

struct SMAS_LabelSlot {
    size_t linkingUnit;
    int section;
    size_t extraOffset;
    int negativeOffset;
    size_t jmpOffset;
    union SM_CodeBlock ** cbdata;
    size_t cbdata_index;
    const struct SMAS_Token * token; /* NULL if this slot is already filled */
};

int SMAS_LabelSlot_filled(struct SMAS_LabelSlot * s, struct SMAS_LabelSlot ** d) {
    assert(s);
    if (s->token == NULL)
        return 1;
    (*d) = s;
    return 0;
}

SM_VECTOR_DECLARE(SMAS_LabelSlots,struct SMAS_LabelSlot,)
SM_VECTOR_DEFINE(SMAS_LabelSlots,struct SMAS_LabelSlot,malloc,free,realloc)
SM_VECTOR_DECLARE_FOREACH_WITH(SMAS_LabelSlots,struct SMAS_LabelSlot,labelLocationPointer,struct SMAS_LabelLocation *,struct SMAS_LabelLocation * p)
SM_VECTOR_DEFINE_FOREACH_WITH(SMAS_LabelSlots,struct SMAS_LabelSlot,labelLocationPointer,struct SMAS_LabelLocation *,struct SMAS_LabelLocation * p,p)
SM_VECTOR_DECLARE_FOREACH_WITH(SMAS_LabelSlots,struct SMAS_LabelSlot,labelSlotPointerPointer,struct SMAS_LabelSlot **,struct SMAS_LabelSlot ** p)
SM_VECTOR_DEFINE_FOREACH_WITH(SMAS_LabelSlots,struct SMAS_LabelSlot,labelSlotPointerPointer,struct SMAS_LabelSlot **,struct SMAS_LabelSlot ** p,p)

int SMAS_LabelSlots_allSlotsFilled(struct SMAS_LabelSlots * ss, struct SMAS_LabelSlot ** d) {
    return SMAS_LabelSlots_foreach_with_labelSlotPointerPointer(ss, &SMAS_LabelSlot_filled, d);
}

SM_TRIE_DECLARE(SMAS_LabelSlotsTrie,struct SMAS_LabelSlots)
SM_TRIE_DEFINE(SMAS_LabelSlotsTrie,struct SMAS_LabelSlots,malloc,free)
SM_TRIE_DECLARE_FOREACH_WITH(SMAS_LabelSlotsTrie,struct SMAS_LabelSlots,labelSlotPointerPointer,struct SMAS_LabelSlot **,struct SMAS_LabelSlot ** p)
SM_TRIE_DEFINE_FOREACH_WITH(SMAS_LabelSlotsTrie,struct SMAS_LabelSlots,labelSlotPointerPointer,struct SMAS_LabelSlot **,struct SMAS_LabelSlot ** p,p)

int SMAS_LabelSlot_fill(struct SMAS_LabelSlot * s, struct SMAS_LabelLocation * l) {
    assert(s);
    assert(s->token);
    assert(l);

    size_t absTarget = l->offset;
    if (s->negativeOffset % 2) {
        absTarget -= s->extraOffset; /**< \todo check overflow/underflow? */
    } else {
        absTarget += s->extraOffset; /**< \todo check overflow/underflow? */
    }

    if (s->negativeOffset < 2) { /* Normal absolute label */
        (*s->cbdata)[s->cbdata_index].uint64[0] = absTarget;
    } else { /* Relative jump label */
        if (s->linkingUnit != l->linkingUnit)
            return 0;

        if (s->section != l->section)
            return 0;

        assert(s->section == SME_SECTION_TYPE_TEXT);
        assert(s->jmpOffset < l->offset); /* Because we're one-pass. */

        /** \todo Maybe check whether there's really an instruction there */
        (*s->cbdata)[s->cbdata_index].int64[0] = absTarget - s->jmpOffset;  /**< \todo check underflow? */
    }
    s->token = NULL;
    return 1;
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
                                       const struct SMAS_Token ** errorToken)
{
    assert(ts);
    assert(lus);
    assert(lus->size == 0u);

    int returnStatus;
    *errorToken = NULL;

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
    size_t multiplier;
    unsigned type;
    static const size_t widths[9] = { 1u, 2u, 4u, 8u, 1u, 2u, 4u, 8u, 4u };

    /* for error messages: */
    char * tmp = NULL;

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
                if (unlikely(t->type != SMAS_TOKEN_HEX))
                    goto smas_assemble_invalid_parameter_t;

                uint64_t v = SMAS_token_hex_value(t);
                if (unlikely(v >= 256))
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

                if (unlikely(t->type != SMAS_TOKEN_HEX))
                    goto smas_assemble_invalid_parameter_t;

                if (unlikely(t->text[0] == '-'))
                    goto smas_assemble_invalid_parameter_t;

                uint64_t m = SMAS_token_hex_value(t);
                if (unlikely(m >= 65536))
                    goto smas_assemble_invalid_parameter_t;

                multiplier = m;

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
            char * name = malloc(sizeof(char) * (l + 1u));
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
                } else if (likely(t->type == SMAS_TOKEN_HEX || t->type == SMAS_TOKEN_LABEL || t->type == SMAS_TOKEN_LABEL_O)) {
                    args++;
                } else {
                    goto smas_assemble_invalid_parameter_t;
                }
            }
            name[l] = '\0';

            /* Detect and check instruction: */
            const struct SMVMI_Instruction * i = SMVMI_Instruction_from_name(name);
            free(name);
            if (unlikely(!i)) {
                *errorToken = ot;
                goto smas_assemble_unknown_instruction;
            }
            if (unlikely(i->numargs != args))
                goto smas_assemble_invalid_parameter_t;

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
            char * newData = realloc(lu->sections[section_index].data, sizeof(union SM_CodeBlock) * (lu->sections[section_index].length + args + 1));
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
                if (ot->type == SMAS_TOKEN_HEX) {
                    doJumpLabel = 0; /* Past first argument */
                    instr++;
                    if (unlikely(ot->text[0] == '-')) {
                        instr->int64[0] = -SMAS_token_hex_value(ot);
                    } else {
                        instr->uint64[0] = SMAS_token_hex_value(ot);
                    }
                } else if (likely(ot->type == SMAS_TOKEN_LABEL || ot->type == SMAS_TOKEN_LABEL_O)) {
                    instr++;
                    char * label = SMAS_token_label_label_new(ot);
                    if (unlikely(!label))
                        goto smas_assemble_out_of_memory;

                    struct SMAS_LabelLocation * loc = SMAS_LabelLocations_find(&ll, label);
                    int negative;
                    uint64_t labelOffset = SMAS_token_label_offset(ot, &negative);
                    if (loc) {
                        free(label);

                        size_t absTarget = loc->offset;
                        if (negative) {
                            absTarget -= labelOffset; /**< \todo check overflow/underflow? */
                        } else {
                            absTarget += labelOffset; /**< \todo check overflow/underflow? */
                        }

                        if (doJumpLabel) {
                            assert(jmpOffset >= loc->offset); /* Because we're one-pass. */

                            if (loc->linkingUnit != lu_index) {
                                *errorToken = ot;
                                goto smas_assemble_invalid_label;
                            }

                            assert(section_index == SME_SECTION_TYPE_TEXT);
                            if (loc->section != section_index) {
                                *errorToken = ot;
                                goto smas_assemble_invalid_label;
                            }

                            /** \todo Maybe check whether there's really an instruction there */
                            instr->int64[0] = absTarget - jmpOffset;  /**< \todo check underflow? */
                        } else {
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
                        slot->extraOffset = labelOffset;
                        slot->negativeOffset = negative;
                        slot->negativeOffset += doJumpLabel * 2; /* Signal a relative jump label */
                        slot->jmpOffset = jmpOffset;
                        slot->cbdata = &lu->sections[section_index].cbdata;
                        slot->cbdata_index = instr - lu->sections[section_index].cbdata;
                        slot->token = ot;
                    }
                    doJumpLabel = 0; /* Past first argument */
                } else {
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
        char * undefinedSlotName = SMAS_token_label_label_new(undefinedSlot->token);
        fprintf(stderr, "Undefined label: %s\n", undefinedSlotName);
        free(undefinedSlotName);

        *errorToken = undefinedSlot->token;
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
    } else if (likely(t->length == 7u && strncmp(t->text, "float32", t->length) == 0)) {
        type = 8u;
    } else {
        goto smas_assemble_invalid_parameter_t;
    }

    SMAS_ASSEMBLE_INC_DO_EOL(smas_assemble_ok,smas_assemble_data_opt_param);

smas_assemble_data_opt_param:

    if (unlikely(t->type != SMAS_TOKEN_HEX))
        goto smas_assemble_invalid_parameter_t;

    int neg = (t->text[0] == '-');
    uint64_t v = SMAS_token_hex_value(t);
    switch (type) {
        case 0u: /* uint8 */
            if (neg || v > 255u)
                goto smas_assemble_invalid_parameter_t;
            break;
        case 1u: /* uint16 */
            if (neg || v > 65535u)
                goto smas_assemble_invalid_parameter_t;
            break;
        case 2u: /* uint32 */
            if (neg || v > 16777216u)
                goto smas_assemble_invalid_parameter_t;
            break;
        case 3u: /* uint64 */
            if (neg)
                goto smas_assemble_invalid_parameter_t;
            break;
        case 4u: /* int8 */
            if ((neg && v > 128u) || (!neg && v > 127u))
                goto smas_assemble_invalid_parameter_t;
            break;
        case 5u: /* int16 */
            if ((neg && v > 32768u) || (!neg && v > 32767u))
                goto smas_assemble_invalid_parameter_t;
            break;
        case 6u: /* int32 */
            if ((neg && v > 2147483648u) || (!neg && v > 2147483647u))
                goto smas_assemble_invalid_parameter_t;
            break;
        case 7u: /* int64 */
            /* All tokenized values should be OK. */
            break;
        case 8u: /* float32 */
            if (neg || v > 16777216u)
                goto smas_assemble_invalid_parameter_t;
            break;
        default:
            abort();
    }

    SMAS_ASSEMBLE_INC_CHECK_EOF(smas_assemble_unexpected_eof);
    lu->sections[section_index].length += (multiplier * widths[type]);
    goto smas_assemble_newline;

smas_assemble_ok:
    returnStatus = SMAS_ASSEMBLE_OK;
    goto smas_assemble_free_and_return;

smas_assemble_out_of_memory:
    returnStatus = SMAS_ASSEMBLE_OUT_OF_MEMORY;
    goto smas_assemble_free_and_return;

smas_assemble_unexpected_token_t:
    *errorToken = t;
    tmp = malloc(t->length + 1);
    strncpy(tmp, t->text, t->length);
    tmp[t->length] = '\0';
    const char * tokenStr = SMAS_TokenType_toString(t->type);
    assert(tokenStr);
    fprintf(stderr, "Unexpected %s(%s)\n", tokenStr, tmp);
    free(tmp);
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

smas_assemble_free_and_return:
    SMAS_LabelSlotsTrie_destroy_with(&lst, &SMAS_LabelSlots_destroy);
    SMAS_LabelLocations_destroy(&ll);
    return returnStatus;
}
