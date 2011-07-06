#include "assemble.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../vm/instr.h"
#include "../likely.h"
#include "../trie.h"
#include "tokens.h"


struct SMA_LabelLocation {
    size_t linkingUnit;
    size_t section;
    size_t offset;
};

SVM_TRIE_DECLARE(SMA_LabelLocations,struct SMA_LabelLocation)
SVM_TRIE_DEFINE(SMA_LabelLocations,struct SMA_LabelLocation,malloc,free)

struct SMA_LabelSlot {
    size_t linkingUnit;
    size_t section;
    size_t extraOffset;
    int negativeOffset;
    union SVM_IBlock * cbdata;
    const struct SMA_Token * token; /* NULL if this slot is already filled */
};

int SMA_LabelSlot_filled(struct SMA_LabelSlot * s, struct SMA_LabelSlot ** d) {
    assert(s);
    if (s->token == NULL)
        return 1;
    (*d) = s;
    return 0;
}

SVM_VECTOR_DECLARE(SMA_LabelSlots,struct SMA_LabelSlot,)
SVM_VECTOR_DEFINE(SMA_LabelSlots,struct SMA_LabelSlot,malloc,free,realloc)
SVM_VECTOR_DECLARE_FOREACH_WITH(SMA_LabelSlots,struct SMA_LabelSlot,labelLocationPointer,struct SMA_LabelLocation *,struct SMA_LabelLocation * p)
SVM_VECTOR_DEFINE_FOREACH_WITH(SMA_LabelSlots,struct SMA_LabelSlot,labelLocationPointer,struct SMA_LabelLocation *,struct SMA_LabelLocation * p,p)
SVM_VECTOR_DECLARE_FOREACH_WITH(SMA_LabelSlots,struct SMA_LabelSlot,labelSlotPointerPointer,struct SMA_LabelSlot **,struct SMA_LabelSlot ** p)
SVM_VECTOR_DEFINE_FOREACH_WITH(SMA_LabelSlots,struct SMA_LabelSlot,labelSlotPointerPointer,struct SMA_LabelSlot **,struct SMA_LabelSlot ** p,p)

int SMA_LabelSlots_allSlotsFilled(struct SMA_LabelSlots * ss, struct SMA_LabelSlot ** d) {
    return SMA_LabelSlots_foreach_with_labelSlotPointerPointer(ss, &SMA_LabelSlot_filled, d);
}

SVM_TRIE_DECLARE(SMA_LabelSlotsTrie,struct SMA_LabelSlots)
SVM_TRIE_DEFINE(SMA_LabelSlotsTrie,struct SMA_LabelSlots,malloc,free)
SVM_TRIE_DECLARE_FOREACH_WITH(SMA_LabelSlotsTrie,struct SMA_LabelSlots,labelSlotPointerPointer,struct SMA_LabelSlot **,struct SMA_LabelSlot ** p)
SVM_TRIE_DEFINE_FOREACH_WITH(SMA_LabelSlotsTrie,struct SMA_LabelSlots,labelSlotPointerPointer,struct SMA_LabelSlot **,struct SMA_LabelSlot ** p,p)

int SMA_LabelSlot_fill(struct SMA_LabelSlot * s, struct SMA_LabelLocation * l) {
    assert(s);
    assert(s->token);
    assert(l);
    s->cbdata->sizet[0] = s->extraOffset;
    if (s->negativeOffset) {
        s->cbdata->sizet[0] -= l->offset; /**< \todo check overflow? */
    } else {
        s->cbdata->sizet[0] += l->offset; /**< \todo check overflow? */
    }
    s->token = NULL;
    return 1;
}

SVM_ENUM_CUSTOM_DEFINE_TOSTRING(SMA_Assemble_Error, SMA_ENUM_Assemble_Error);

#define SMA_ASSEMBLE_EOF_TEST     (unlikely(  t >= e))
#define SMA_ASSEMBLE_INC_EOF_TEST (unlikely(++t >= e))

#define SMA_ASSEMBLE_INC_CHECK_EOF(eof) \
    if (SMA_ASSEMBLE_INC_EOF_TEST) { \
        goto eof; \
    } else (void) 0

#define SMA_ASSEMBLE_DO_EOL(eof,noexpect) \
    if (1) { \
        if (SMA_ASSEMBLE_EOF_TEST) \
            goto eof; \
        if (unlikely(t->type != SMA_TOKEN_NEWLINE)) \
            goto noexpect; \
        goto sma_assemble_newline; \
    } else (void) 0

#define SMA_ASSEMBLE_INC_DO_EOL(eof,noexpect) \
    if (1) { \
        t++; \
        SMA_ASSEMBLE_DO_EOL(eof,noexpect); \
    } else (void) 0

enum SMA_Assemble_Error SMA_assemble(const struct SMA_Tokens * ts,
                                     struct SMA_LinkingUnits * lus)
{
    assert(ts);
    assert(lus);
    assert(lus->size == 0u);

    int returnStatus;

    struct SMA_LabelLocations ll;
    SMA_LabelLocations_init(&ll);

    struct SMA_LabelSlotsTrie lst;
    SMA_LabelSlotsTrie_init(&lst);

    struct SMA_LinkingUnit * lu = SMA_LinkingUnits_push(lus);
    if (unlikely(!lu))
        goto sma_assemble_out_of_memory;

    SMA_LinkingUnit_init(lu);

    if (unlikely(ts->numTokens <= 0))
        goto sma_assemble_ok;

    struct SMA_Token * t = &ts->array[0u];
    struct SMA_Token * e = &ts->array[ts->numTokens];

    size_t lu_index = 0u;
    int section_index = SMA_SECTION_TYPE_TEXT;

    /* for .data and .fill: */
    size_t multiplier;
    unsigned type;
    static const size_t widths[9] = { 1u, 2u, 4u, 8u, 1u, 2u, 4u, 8u, 4u };

    /* for error messages: */
    char * tmp = NULL;

sma_assemble_newline:
    switch (t->type) {
        case SMA_TOKEN_NEWLINE:
            break;
        case SMA_TOKEN_LABEL:
        {
            char * label = SMA_token_label_label_new(t);
            if (unlikely(!label))
                    goto sma_assemble_out_of_memory;

            int newValue;
            struct SMA_LabelLocation * l = SMA_LabelLocations_get_or_insert(&ll, label, &newValue);
            if (unlikely(!l)) {
                free(label);
                goto sma_assemble_out_of_memory;
            }

            /* Check for duplicate label: */
            if (unlikely(!newValue)) {
                free(label);
                goto sma_assemble_duplicate_label;
            }

            l->linkingUnit = lu_index;
            l->section = section_index;
            l->offset = lu->sections[section_index].length;

            /* Fill pending label slots: */
            struct SMA_LabelSlots * slots = SMA_LabelSlotsTrie_find(&lst, label);
            free(label);
            if (slots) {
                SMA_LabelSlots_foreach_with_labelLocationPointer(slots, &SMA_LabelSlot_fill, l);
            }
            break;
        }
        case SMA_TOKEN_DIRECTIVE:
            if (t->length == 13u && strncmp(t->text, ".linking_unit", t->length) == 0) {
                SMA_ASSEMBLE_INC_CHECK_EOF(sma_assemble_unexpected_eof);
                if (unlikely(t->type != SMA_TOKEN_HEX))
                    goto sma_assemble_invalid_parameter;

                uint64_t v = SMA_token_hex_value(t);
                if (unlikely(v >= 256))
                    goto sma_assemble_invalid_parameter;

                if (likely(v != lu_index)) {
                    if (unlikely(v > lus->size))
                        goto sma_assemble_invalid_parameter;
                    if (v == lus->size) {
                        lu = SMA_LinkingUnits_push(lus);
                        if (unlikely(!lu))
                            goto sma_assemble_out_of_memory;
                    } else {
                        lu = SMA_LinkingUnits_get_pointer(lus, v);
                    }
                    lu_index = v;
                    section_index = SMA_SECTION_TYPE_TEXT;
                }

                SMA_ASSEMBLE_INC_DO_EOL(sma_assemble_check_labels,sma_assemble_unexpected_token);
            } else if (t->length == 8u && strncmp(t->text, ".section", t->length) == 0) {
                SMA_ASSEMBLE_INC_CHECK_EOF(sma_assemble_unexpected_eof);
                if (unlikely(t->type != SMA_TOKEN_KEYWORD))
                    goto sma_assemble_invalid_parameter;

                if (t->length == 4u && strncmp(t->text, "TEXT", t->length) == 0) {
                    section_index = SMA_SECTION_TYPE_TEXT;
                } else if (t->length == 6u && strncmp(t->text, "RODATA", t->length) == 0) {
                    section_index = SMA_SECTION_TYPE_RODATA;
                } else if (t->length == 4u && strncmp(t->text, "DATA", t->length) == 0) {
                    section_index = SMA_SECTION_TYPE_DATA;
                } else if (t->length == 3u && strncmp(t->text, "BSS", t->length) == 0) {
                    section_index = SMA_SECTION_TYPE_BSS;
                } else if (t->length == 4u && strncmp(t->text, "BIND", t->length) == 0) {
                    section_index = SMA_SECTION_TYPE_BIND;
                } else if (t->length == 5u && strncmp(t->text, "DEBUG", t->length) == 0) {
                    section_index = SMA_SECTION_TYPE_DEBUG;
                } else {
                    goto sma_assemble_invalid_parameter;
                }
            } else if (t->length == 5u && strncmp(t->text, ".data", t->length) == 0) {
                if (unlikely(section_index == SMA_SECTION_TYPE_TEXT))
                    goto sma_assemble_unexpected_token;

                multiplier = 1u;
                goto sma_assemble_data_or_fill;
            } else if (t->length == 5u && strncmp(t->text, ".fill", t->length) == 0) {
                if (unlikely(section_index == SMA_SECTION_TYPE_TEXT || section_index == SMA_SECTION_TYPE_BIND))
                    goto sma_assemble_unexpected_token;

                SMA_ASSEMBLE_INC_CHECK_EOF(sma_assemble_unexpected_eof);

                if (unlikely(t->type != SMA_TOKEN_HEX))
                    goto sma_assemble_invalid_parameter;

                if (unlikely(t->text[0] == '-'))
                    goto sma_assemble_invalid_parameter;

                uint64_t m = SMA_token_hex_value(t);
                if (unlikely(m >= 65536))
                    goto sma_assemble_invalid_parameter;

                multiplier = m;

                goto sma_assemble_data_or_fill;
            } else if (likely(t->length == 13u && strncmp(t->text, ".bind_syscall", t->length) == 0)) {
                if (unlikely(section_index != SMA_SECTION_TYPE_BIND))
                    goto sma_assemble_unexpected_token;

                fprintf(stderr, "TODO\n");
                goto sma_assemble_unknown_directive;
            } else {
                goto sma_assemble_unknown_directive;
            }
            break;
        case SMA_TOKEN_KEYWORD:
        {
            size_t args = 0u;
            size_t l = t->length;
            char * name = malloc(sizeof(char) * (l + 1u));
            if (unlikely(!name))
                goto sma_assemble_out_of_memory;
            strncpy(name, t->text, l);

            const struct SMA_Token * ot = t;
            /* Collect instruction name and count arguments: */
            for (;;) {
                if (SMA_ASSEMBLE_INC_EOF_TEST)
                    break;
                if (t->type == SMA_TOKEN_NEWLINE) {
                    break;
                } else if (t->type == SMA_TOKEN_KEYWORD) {
                    size_t newSize = l + t->length + 1u;
                    if (unlikely(newSize < l))
                        goto sma_assemble_invalid_parameter;
                    char * newName = (char *) realloc(name, sizeof(char) * (newSize + 1u));
                    if (unlikely(!newName)) {
                        free(name);
                        goto sma_assemble_out_of_memory;
                    }
                    name = newName;
                    name[l] = '_';
                    strncpy(name + l + 1u, t->text, t->length);
                    l = newSize;
                } else if (likely(t->type == SMA_TOKEN_HEX || t->type == SMA_TOKEN_LABEL || t->type == SMA_TOKEN_LABEL_O)) {
                    args++;
                } else {
                    goto sma_assemble_invalid_parameter;
                }
            }
            name[l] = '\0';

            /* Detect and check instruction: */
            const struct SVM_Instruction * i = SVM_Instruction_from_name(name);
            free(name);
            if (unlikely(!i))
                goto sma_assemble_unknown_instruction;
            if (unlikely(i->numargs != args))
                goto sma_assemble_invalid_parameter;

            /* Allocate whole instruction: */
            char * newData = realloc(lu->sections[section_index].data, sizeof(union SVM_IBlock) * (lu->sections[section_index].length + args + 1));
            if (unlikely(!newData))
                goto sma_assemble_out_of_memory;
            lu->sections[section_index].data = newData;
            union SVM_IBlock * instr = &lu->sections[section_index].cbdata[lu->sections[section_index].length];
            lu->sections[section_index].length += args + 1;

            /* Write instruction code */
            instr->uint64[0] = i->code;

            /* Write arguments: */
            for (;;) {
                if (++ot == t)
                    break;
                if (ot->type == SMA_TOKEN_HEX) {
                    instr++;
                    if (unlikely(ot->text[0] == '-')) {
                        instr->int64[0] = -SMA_token_hex_value(ot);
                    } else {
                        instr->uint64[0] = SMA_token_hex_value(ot);
                    }
                } else if (likely(ot->type == SMA_TOKEN_LABEL || ot->type == SMA_TOKEN_LABEL_O)) {
                    instr++;
                    char * label = SMA_token_label_label_new(ot);
                    if (unlikely(!label))
                        goto sma_assemble_out_of_memory;

                    struct SMA_LabelLocation * loc = SMA_LabelLocations_find(&ll, label);
                    int negative;
                    uint64_t labelOffset = SMA_token_label_offset(ot, negative);
                    if (loc) {
                        free(label);
                        instr->sizet[0] = loc->offset;
                        if (negative) {
                            instr->sizet[0] -= labelOffset; /**< \todo check overflow? */
                        } else {
                            instr->sizet[0] += labelOffset; /**< \todo check overflow? */
                        }
                    } else {
                        int newValue;
                        struct SMA_LabelSlots * slots = SMA_LabelSlotsTrie_get_or_insert(&lst, label, &newValue);
                        free(label);
                        if (unlikely(!slots))
                            goto sma_assemble_out_of_memory;

                        if (newValue)
                            SMA_LabelSlots_init(slots);

                        struct SMA_LabelSlot * slot = SMA_LabelSlots_push(slots);
                        if (unlikely(!slot))
                            goto sma_assemble_out_of_memory;

                        slot->linkingUnit = lu_index;
                        slot->section = section_index;
                        slot->extraOffset = labelOffset;
                        slot->negativeOffset = negative;
                        slot->cbdata = instr;
                        slot->token = ot;
                    }
                } else {
                    assert(ot->type == SMA_TOKEN_KEYWORD);
                }
            }

            SMA_ASSEMBLE_DO_EOL(sma_assemble_check_labels,sma_assemble_unexpected_token);
            abort();
        }
        default:
            goto sma_assemble_unexpected_token;
    } /* switch */

    if (!SMA_ASSEMBLE_INC_EOF_TEST)
        goto sma_assemble_newline;

sma_assemble_check_labels:

    /* Check for undefined labels: */
    {
        struct SMA_LabelSlot * undefinedSlot;
        if (likely(SMA_LabelSlotsTrie_foreach_with_labelSlotPointerPointer(&lst, &SMA_LabelSlots_allSlotsFilled, &undefinedSlot)))
            goto sma_assemble_ok;

        assert(undefinedSlot);
        char * undefinedSlotName = SMA_token_label_label_new(undefinedSlot->token);
        fprintf(stderr, "Undefined label: %s\n", undefinedSlotName);
        free(undefinedSlotName);
        goto sma_assemble_undefined_label;
    }

sma_assemble_data_or_fill:

    SMA_ASSEMBLE_INC_CHECK_EOF(sma_assemble_unexpected_eof);

    if (unlikely(t->type != SMA_TOKEN_KEYWORD))
        goto sma_assemble_invalid_parameter;

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
        goto sma_assemble_invalid_parameter;
    }

    SMA_ASSEMBLE_INC_DO_EOL(sma_assemble_ok,sma_assemble_data_opt_param);

sma_assemble_data_opt_param:

    if (unlikely(t->type != SMA_TOKEN_HEX))
        goto sma_assemble_invalid_parameter;

    int neg = (t->text[0] == '-');
    uint64_t v = SMA_token_hex_value(t);
    switch (type) {
        case 0u: /* uint8 */
            if (neg || v > 255u)
                goto sma_assemble_invalid_parameter;
            break;
        case 1u: /* uint16 */
            if (neg || v > 65535u)
                goto sma_assemble_invalid_parameter;
            break;
        case 2u: /* uint32 */
            if (neg || v > 16777216u)
                goto sma_assemble_invalid_parameter;
            break;
        case 3u: /* uint64 */
            if (neg)
                goto sma_assemble_invalid_parameter;
            break;
        case 4u: /* int8 */
            if ((neg && v > 128u) || (!neg && v > 127u))
                goto sma_assemble_invalid_parameter;
            break;
        case 5u: /* int16 */
            if ((neg && v > 32768u) || (!neg && v > 32767u))
                goto sma_assemble_invalid_parameter;
            break;
        case 6u: /* int32 */
            if ((neg && v > 2147483648u) || (!neg && v > 2147483647u))
                goto sma_assemble_invalid_parameter;
            break;
        case 7u: /* int64 */
            /* All tokenized values should be OK. */
            break;
        case 8u: /* float32 */
            if (neg || v > 16777216u)
                goto sma_assemble_invalid_parameter;
            break;
        default:
            abort();
    }

    SMA_ASSEMBLE_INC_CHECK_EOF(sma_assemble_unexpected_eof);
    lu->sections[section_index].length += (multiplier * widths[type]);
    goto sma_assemble_newline;

sma_assemble_ok:
    returnStatus = SMA_ASSEMBLE_OK;
    goto sma_assemble_free_and_return;

sma_assemble_out_of_memory:
    returnStatus = SMA_ASSEMBLE_OUT_OF_MEMORY;
    goto sma_assemble_free_and_return;

sma_assemble_unexpected_token:
    tmp = malloc(t->length + 1);
    strncpy(tmp, t->text, t->length);
    tmp[t->length] = '\0';
    fprintf(stderr, "Unexpected %s(%s)\n", SMA_TokenType_toString(t->type), tmp);
    free(tmp);
    returnStatus = SMA_ASSEMBLE_UNEXPECTED_TOKEN;
    goto sma_assemble_free_and_return;

sma_assemble_unexpected_eof:
    returnStatus = SMA_ASSEMBLE_UNEXPECTED_EOF;
    goto sma_assemble_free_and_return;

sma_assemble_duplicate_label:
    returnStatus = SMA_ASSEMBLE_DUPLICATE_LABEL;
    goto sma_assemble_free_and_return;

sma_assemble_unknown_directive:
    returnStatus = SMA_ASSEMBLE_UNKNOWN_DIRECTIVE;
    goto sma_assemble_free_and_return;

sma_assemble_unknown_instruction:
    returnStatus = SMA_ASSEMBLE_UNKNOWN_INSTRUCTION;
    goto sma_assemble_free_and_return;

sma_assemble_invalid_parameter:
    returnStatus = SMA_ASSEMBLE_INVALID_PARAMETER;
    goto sma_assemble_free_and_return;

sma_assemble_undefined_label:
    returnStatus = SMA_ASSEMBLE_UNDEFINED_LABEL;
    goto sma_assemble_free_and_return;

sma_assemble_free_and_return:
    SMA_LabelSlotsTrie_destroy_with(&lst, &SMA_LabelSlots_destroy);
    SMA_LabelLocations_destroy(&ll);
    return returnStatus;
}
