#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../vm/instr.h"
#include "../preprocessor.h"
#include "../trie.h"
#include "../vector.h"
#include "tokens.h"
#include "tokenizer.h"

struct Label {
    size_t linkingUnit;
    size_t section;
    size_t offset;
};

SVM_TRIE_DECLARE(LabelLocations,struct Label)
SVM_TRIE_DEFINE(LabelLocations,struct Label,malloc,free)

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
    char * data;
};

void Section_init(struct Section * s) {
    s->length = 0u;
    s->data = NULL;
}

void Section_destroy(struct Section * s) {
    free(s->data);
}

struct LinkingUnit {
    struct Section sections[SECTION_TYPE_COUNT];
};

void LinkingUnit_init(struct LinkingUnit * lu) {
    for (size_t i = 0u; i < SECTION_TYPE_COUNT; i++)
        Section_init(&lu->sections[i]);
}

void LinkingUnit_destroy(struct LinkingUnit * lu) {
    for (size_t i = 0u; i < SECTION_TYPE_COUNT; i++)
        Section_destroy(&lu->sections[i]);
}

SVM_VECTOR_DECLARE(LinkingUnits,struct LinkingUnit,)
SVM_VECTOR_DEFINE(LinkingUnits,struct LinkingUnit,malloc,free,realloc)

#define SMA_ENUM_Pass_One_Error \
    ((PASS_ONE_OK, = 0)) \
    ((PASS_ONE_OUT_OF_MEMORY,)) \
    ((PASS_ONE_UNEXPECTED_TOKEN,)) \
    ((PASS_ONE_UNEXPECTED_EOF,)) \
    ((PASS_ONE_DUPLICATE_LABEL,)) \
    ((PASS_ONE_UNKNOWN_DIRECTIVE,)) \
    ((PASS_ONE_UNKNOWN_INSTRUCTION,)) \
    ((PASS_ONE_INVALID_PARAMETER,)) \
    ((PASS_ONE_INVALID_ALIGNMENT,))
SVM_ENUM_CUSTOM_DEFINE(SMA_Pass_One_Error , SMA_ENUM_Pass_One_Error);
SVM_ENUM_DECLARE_TOSTRING(SMA_Pass_One_Error);
SVM_ENUM_CUSTOM_DEFINE_TOSTRING(SMA_Pass_One_Error, SMA_ENUM_Pass_One_Error);

#define PASS_ONE_INC_CHECK_EOF(eof) \
    if (++t == e) { \
        goto eof; \
    } else (void) 0

#define PASS_ONE_DO_EOL(eof,noexpect) \
    if (1) { \
        if (t >= e) \
            goto eof; \
        if (t->type != TOKEN_NEWLINE) \
            goto noexpect; \
        goto pass_one_newline; \
    } else (void) 0

#define PASS_ONE_INC_DO_EOL(eof,noexpect) \
    if (1) { \
        t++; \
        PASS_ONE_DO_EOL(eof,noexpect); \
    } else (void) 0

int pass_one(const struct Tokens * ts, struct LinkingUnits * lus, struct LabelLocations * ll) {
    assert(ts);
    assert(lus);
    assert(lus->size == 0u);
    assert(ll);

    struct LinkingUnit * lu = LinkingUnits_push(lus);
    if (!lu)
        return PASS_ONE_OUT_OF_MEMORY;

    LinkingUnit_init(lu);

    if (ts->numTokens <= 0)
        return PASS_ONE_OK;

    struct Token * t = &ts->array[0u];
    struct Token * e = &ts->array[ts->numTokens];

    size_t lu_index = 0u;
    int section_index = SECTION_TYPE_TEXT;

    /* for .data and .fill: */
    size_t multiplier;
    unsigned type;
    static const size_t widths[9] = { 1u, 2u, 4u, 8u, 1u, 2u, 4u, 8u, 4u };

    /* for error messages: */
    char * tmp = NULL;

pass_one_newline:
    switch (t->type) {
        case TOKEN_NEWLINE:
            break;
        case TOKEN_LABEL:
        {
            char * label = malloc(t->length + 1);
            if (!label)
                    goto pass_one_out_of_memory;
            strncpy(label, t->text, t->length);
            label[t->length] = '\x00';

            if (LabelLocations_find(ll, label)) {
                free(label);
                goto pass_one_duplicate_label;
            } else {
                struct Label * l = LabelLocations_get_or_insert(ll, label);
                free(label);
                if (!l)
                    goto pass_one_out_of_memory;
                l->linkingUnit = lu_index;
                l->section = section_index;
                l->offset = lu->sections[section_index].length;
            }
            break;
        }
        case TOKEN_DIRECTIVE:
            if (t->length == 13u && strncmp(t->text, ".linking_unit", t->length) == 0) {
                PASS_ONE_INC_CHECK_EOF(pass_one_unexpected_eof);
                if (t->type != TOKEN_HEX)
                    goto pass_one_invalid_parameter;

                uint64_t v = token_hex_value(t);
                if (v >= 256)
                    goto pass_one_invalid_parameter;

                if (v != lu_index) {
                    if (v > lus->size)
                        goto pass_one_invalid_parameter;
                    if (v == lus->size) {
                        lu = LinkingUnits_push(lus);
                        if (!lu)
                            goto pass_one_out_of_memory;
                    } else {
                        lu = LinkingUnits_get_pointer(lus, v);
                    }
                    lu_index = v;
                    section_index = SECTION_TYPE_TEXT;
                }

                PASS_ONE_INC_DO_EOL(pass_one_ok,pass_one_unexpected_token);
            } else if (t->length == 8u && strncmp(t->text, ".section", t->length) == 0) {
                PASS_ONE_INC_CHECK_EOF(pass_one_unexpected_eof);
                if (t->type != TOKEN_KEYWORD)
                    goto pass_one_invalid_parameter;

                if (t->length == 4u && strncmp(t->text, "TEXT", t->length) == 0) {
                    section_index = SECTION_TYPE_TEXT;
                } else if (t->length == 6u && strncmp(t->text, "RODATA", t->length) == 0) {
                    section_index = SECTION_TYPE_RODATA;
                } else if (t->length == 4u && strncmp(t->text, "DATA", t->length) == 0) {
                    section_index = SECTION_TYPE_DATA;
                } else if (t->length == 3u && strncmp(t->text, "BSS", t->length) == 0) {
                    section_index = SECTION_TYPE_BSS;
                } else if (t->length == 4u && strncmp(t->text, "BIND", t->length) == 0) {
                    section_index = SECTION_TYPE_BIND;
                } else if (t->length == 5u && strncmp(t->text, "DEBUG", t->length) == 0) {
                    section_index = SECTION_TYPE_DEBUG;
                } else {
                    goto pass_one_invalid_parameter;
                }
            } else if (t->length == 5u && strncmp(t->text, ".data", t->length) == 0) {
                multiplier = 1u;
                goto pass_one_data_or_fill;
            } else if (t->length == 5u && strncmp(t->text, ".fill", t->length) == 0) {
                PASS_ONE_INC_CHECK_EOF(pass_one_unexpected_eof);

                if (t->type != TOKEN_HEX)
                    goto pass_one_invalid_parameter;

                if (t->text[0] == '-')
                    goto pass_one_invalid_parameter;

                uint64_t m = token_hex_value(t);
                if (m >= 65536)
                    goto pass_one_invalid_parameter;

                multiplier = m;

                goto pass_one_data_or_fill;
            } else if (t->length == 13u && strncmp(t->text, ".bind_syscall", t->length) == 0) {
                fprintf(stderr, "TODO\n");
                abort();
            } else {
                goto pass_one_unknown_directive;
            }
            break;
        case TOKEN_KEYWORD:
        {
            if ((lu->sections[section_index].length % (sizeof(uint64_t))) != 0) {
                printf("Alignment is %lu\n", lu->sections[section_index].length % (sizeof(uint64_t)));
                goto pass_one_invalid_alignment;
            }
            size_t args = 0u;
            size_t l = t->length;
            char * name = (char *) malloc(sizeof(char) * (l + 1u));
            if (!name)
                goto pass_one_out_of_memory;
            strncpy(name, t->text, l);

            for (;;) {
                t++;
                if (t == e)
                    break;
                if (t->type == TOKEN_NEWLINE) {
                    break;
                } else if (t->type == TOKEN_KEYWORD) {
                    size_t newSize = l + t->length + 1u;
                    if (newSize < l)
                        goto pass_one_invalid_parameter;
                    char * newName = (char *) realloc(name, sizeof(char) * (newSize + 1u));
                    if (!newName) {
                        free(name);
                        goto pass_one_out_of_memory;
                    }
                    name = newName;
                    name[l] = '_';
                    strncpy(name + l + 1u, t->text, t->length);
                    l = newSize;
                } else if (t->type == TOKEN_HEX || t->type == TOKEN_LABEL || t->type == TOKEN_LABEL_O) {
                    args++;
                } else {
                    goto pass_one_invalid_parameter;
                }
            }

            name[l] = '\0';
            const struct SVM_Instruction * i = SVM_Instruction_from_name(name);
            free(name);
            if (!i)
                goto pass_one_unknown_instruction;

            if (i->numargs != args)
                goto pass_one_invalid_parameter;

            lu->sections[section_index].length += sizeof(uint64_t) * (args + 1);
            PASS_ONE_DO_EOL(pass_one_ok,pass_one_unexpected_token);
            abort();
        }
        default:
            goto pass_one_unexpected_token;
    } /* switch */

    if (++t == e)
        goto pass_one_ok;

    goto pass_one_newline;

pass_one_data_or_fill:

    PASS_ONE_INC_CHECK_EOF(pass_one_unexpected_eof);

    if (t->type != TOKEN_KEYWORD)
        goto pass_one_invalid_parameter;

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
    } else if (t->length == 7u && strncmp(t->text, "float32", t->length) == 0) {
        type = 8u;
    } else {
        goto pass_one_invalid_parameter;
    }

    PASS_ONE_INC_DO_EOL(pass_one_ok,pass_one_data_opt_param);

pass_one_data_opt_param:

    if (t->type != TOKEN_HEX)
        goto pass_one_invalid_parameter;

    int neg = (t->text[0] == '-');
    uint64_t v = token_hex_value(t);
    switch (type) {
        case 1u: /* uint8 */
            if (neg || v > 255u)
                goto pass_one_invalid_parameter;
            break;
        case 2u: /* uint16 */
            if (neg || v > 65535u)
                goto pass_one_invalid_parameter;
            break;
        case 3u: /* uint32 */
            if (neg || v > 16777216u)
                goto pass_one_invalid_parameter;
            break;
        case 4u: /* uint64 */
            if (neg)
                goto pass_one_invalid_parameter;
            break;
        case 5u: /* int8 */
            if ((neg && v > 128u) || (!neg && v > 127u))
                goto pass_one_invalid_parameter;
            break;
        case 6u: /* int16 */
            if ((neg && v > 32768u) || (!neg && v > 32767u))
                goto pass_one_invalid_parameter;
            break;
        case 7u: /* int32 */
            if ((neg && v > 2147483648u) || (!neg && v > 2147483647u))
                goto pass_one_invalid_parameter;
            break;
        case 8u: /* int64 */
            /* All tokenized values should be OK. */
            break;
        case 9u: /* float32 */
            if (neg || v > 16777216u)
                goto pass_one_invalid_parameter;
            break;
        default:
            abort();
    }

    PASS_ONE_INC_CHECK_EOF(pass_one_unexpected_eof);
    lu->sections[section_index].length += (multiplier * widths[type]);
    goto pass_one_newline;

pass_one_ok:
    return PASS_ONE_OK;

pass_one_out_of_memory:
    return PASS_ONE_OUT_OF_MEMORY;

pass_one_unexpected_token:
    tmp = malloc(t->length + 1);
    strncpy(tmp, t->text, t->length);
    tmp[t->length] = '\0';
    fprintf(stderr, "Unexpected %s(%s)\n", SMA_TokenType_toString(t->type), tmp);
    free(tmp);
    return PASS_ONE_UNEXPECTED_TOKEN;

pass_one_unexpected_eof:
    return PASS_ONE_UNEXPECTED_EOF;

pass_one_duplicate_label:
    return PASS_ONE_DUPLICATE_LABEL;

pass_one_unknown_directive:
    return PASS_ONE_UNKNOWN_DIRECTIVE;

pass_one_unknown_instruction:
    return PASS_ONE_UNKNOWN_INSTRUCTION;

pass_one_invalid_parameter:
    return PASS_ONE_INVALID_PARAMETER;

pass_one_invalid_alignment:
    return PASS_ONE_INVALID_ALIGNMENT;
}



int main() {

    const char * program =
        ":start nop\n"
        "resizestack 0x10\n"
        ":test mov imm 0x256 reg 0x9\n"
        "mov imm 0x257 stack 0x8\n"
        "jmp imm 0x1\n"
        "nop\n"
        "nop\n"
        "push imm 0x255\n"
        "push reg 0x9\n"
        "push stack 0x8\n"
        "halt 0x255\n"
        "nop\n";

    /* Tokenize: */

    size_t sl = 0u;
    size_t sc = 0u;
    struct Tokens * ts = tokenize(program, strlen(program), &sl, &sc);
    if (!ts)
        goto main_tokenize_fail;

    tokens_print(ts);

    /* PASS 1: Get label values */

    struct LinkingUnits lus;
    LinkingUnits_init(&lus);

    struct LabelLocations ll;
    LabelLocations_init(&ll);

    int r = pass_one(ts, &lus, &ll);
    if (r != 0)
        goto main_pass_one_fail;

    /* PASS 2: Assemble */
    /** \todo PASS 2 */

    free(ts);
    return EXIT_SUCCESS;

main_pass_one_fail:

    fprintf(stderr, "Pass 1 failed with %s!\n", SMA_Pass_One_Error_toString(r));
    free(ts);
    return EXIT_FAILURE;

main_tokenize_fail:

    fprintf(stderr, "Tokenization failed at (%lu,%lu)!\n", sl, sc);
    return EXIT_FAILURE;

}
