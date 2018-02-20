/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#include "assemble.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <sharemind/abort.h>
#include <sharemind/comma.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <sharemind/null.h>
#include <sharemind/stringmap.h>


static inline bool assign_add_sizet_int64(size_t * const lhs, const int64_t rhs)
{
    if (rhs > 0) {
        if (((uint64_t) rhs) > SIZE_MAX - (*lhs))
            return false;
        (*lhs) += (uint64_t) rhs;
    } else if (rhs < 0) {
        if (rhs == INT64_MIN) {
            if ((*lhs) <= (uint64_t) INT64_MAX)
                return false;
            (*lhs)--;
            (*lhs) -= (uint64_t) INT64_MAX;
        } else {
            if ((*lhs) < (uint64_t) -rhs)
              return false;
            (*lhs) -= (uint64_t) -rhs;
        }
    }
    return true;
}

static inline bool substract_2sizet_to_int64(
        int64_t * const dest,
        const size_t lhs,
        const size_t rhs)
{
    if (lhs >= rhs) {
        size_t r = lhs - rhs;
        if (r > INT64_MAX)
            return false;
        (*dest) = (int64_t) r;
    } else {
        size_t mr = rhs - lhs;
        assert(mr > 0);
        if (mr - 1 > (uint64_t) INT64_MAX) {
            return false;
        } else if (mr - 1 == (uint64_t) INT64_MAX) {
            (*dest) = INT64_MIN;
        } else {
            (*dest) = -((int64_t) mr);
        }
    }
    return true;
}


typedef struct {
    size_t offset;
    int section;
    uint8_t linkingUnit;
} SharemindAssemblerLabelLocation;

SHAREMIND_STRINGMAP_DECLARE_BODY(SmAsLabelLocationMap,
                                 SharemindAssemblerLabelLocation)

SHAREMIND_STRINGMAP_DEFINE_init(SmAsLabelLocationMap,static inline)
SHAREMIND_STRINGMAP_DEFINE_insertHint(SmAsLabelLocationMap,static inline)
SHAREMIND_STRINGMAP_DEFINE_emplaceAtHint(SmAsLabelLocationMap,static inline)
SHAREMIND_STRINGMAP_DEFINE_insertAtHint(SmAsLabelLocationMap,
                                        static inline,strdup,malloc,free)
SHAREMIND_STRINGMAP_DEFINE_insertNew(SmAsLabelLocationMap,static inline)
SHAREMIND_STRINGMAP_DEFINE_get(SmAsLabelLocationMap,static inline)
SHAREMIND_STRINGMAP_DEFINE_destroy(
        SmAsLabelLocationMap,
        static inline,SharemindAssemblerLabelLocation,,free,)

typedef struct {
    int64_t extraOffset;
    size_t jmpOffset;
    void ** data;
    size_t cbdata_index;
    /* NULL if this slot is already filled: */
    const SharemindAssemblerToken * token;
    int section;
    bool doJumpLabel;
    uint8_t linkingUnit;
} SharemindAssemblerLabelSlot;

SHAREMIND_VECTOR_DECLARE_BODY(SharemindAssemblerLabelSlots,
                              SharemindAssemblerLabelSlot)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindAssemblerLabelSlots,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindAssemblerLabelSlots,static,)
SHAREMIND_VECTOR_DEFINE_INIT(SharemindAssemblerLabelSlots,static)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindAssemblerLabelSlots,static,)
SHAREMIND_VECTOR_DEFINE_DESTROY(SharemindAssemblerLabelSlots,static,free)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindAssemblerLabelSlots,static,)
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindAssemblerLabelSlots,
                                     static,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindAssemblerLabelSlots, static,)
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindAssemblerLabelSlots, static)
SHAREMIND_VECTOR_DECLARE_FOREACH(SharemindAssemblerLabelSlots,
                                 firstUndefinedSlot,
                                 static SharemindAssemblerLabelSlot *,
                                 const,,)
SHAREMIND_VECTOR_DEFINE_FOREACH(
        SharemindAssemblerLabelSlots,
        firstUndefinedSlot,
        static SharemindAssemblerLabelSlot *,
        const,,,
        SHAREMIND_NULL,
        if (value->token != SHAREMIND_NULL)
            return value;)
SHAREMIND_VECTOR_DECLARE_FOREACH(
        SharemindAssemblerLabelSlots,
        fillSlots,
        static bool,,
        SHAREMIND_COMMA SharemindAssemblerLabelLocation *,)
SHAREMIND_VECTOR_DEFINE_FOREACH(
        SharemindAssemblerLabelSlots,
        fillSlots,
        static bool,,
        SHAREMIND_COMMA SharemindAssemblerLabelLocation * l,,
        true,
        assert(value);
        assert(value->token);
        assert(l);

        size_t absTarget = l->offset;
        if (!assign_add_sizet_int64(&absTarget, value->extraOffset))
            return false; /**< \todo Provide better diagnostics */

        if (!value->doJumpLabel) { /* Normal absolute label */
            ((SharemindCodeBlock *) *value->data)[value->cbdata_index].uint64[0]
                    = absTarget;
        } else { /* Relative jump label */
            if (value->section != l->section
                || value->linkingUnit != l->linkingUnit)
                return false; /**< \todo Provide better diagnostics */

            assert(value->section == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT);
            assert(value->jmpOffset < l->offset); /* Because we're one-pass. */

            if (!substract_2sizet_to_int64(
                    &((SharemindCodeBlock *) *value->data)[value->cbdata_index]
                            .int64[0],
                    absTarget,
                    value->jmpOffset))
                return false; /**< \todo Provide better diagnostics */

            /** \todo Maybe check whether there's really an instruction there */
        }
        value->token = SHAREMIND_NULL;)

static bool SharemindAssemblerLabelSlots_all_slots_filled(
        SharemindAssemblerLabelSlots * ss,
        SharemindAssemblerLabelSlot ** d)
{
    SharemindAssemblerLabelSlot * const undefinedSlot =
            SharemindAssemblerLabelSlots_firstUndefinedSlot(ss);
    if (!undefinedSlot)
        return true;
    (*d) = undefinedSlot;
    return false;
}

SHAREMIND_STRINGMAP_DECLARE_BODY(SmAsLabelSlotsMap,SharemindAssemblerLabelSlots)


SHAREMIND_STRINGMAP_DEFINE_init(SmAsLabelSlotsMap,static inline)
SHAREMIND_STRINGMAP_DEFINE_insertHint(SmAsLabelSlotsMap,static inline)
SHAREMIND_STRINGMAP_DEFINE_emplaceAtHint(SmAsLabelSlotsMap,static inline)
SHAREMIND_STRINGMAP_DEFINE_insertAtHint(
        SmAsLabelSlotsMap,static inline,strdup,malloc,free)
SHAREMIND_STRINGMAP_DEFINE_insertNew(SmAsLabelSlotsMap,static inline)
SHAREMIND_STRINGMAP_DEFINE_get(SmAsLabelSlotsMap,static inline)

SHAREMIND_STRINGMAP_DEFINE_destroy(
        SmAsLabelSlotsMap,
        static inline,
        SharemindAssemblerLabelSlots,,
        free,
        SharemindAssemblerLabelSlots_destroy(&v->value);)

static inline SharemindAssemblerLabelSlots*
SmAsLabelSlotsMap_getOrInsertNew(
        SmAsLabelSlotsMap * lst, const char * label)
{
    // TODO: Can be optimized to a single lookup
    SmAsLabelSlotsMap_value * record = SmAsLabelSlotsMap_get(lst, label);
    if (record)
        return &record->value;

    record = SmAsLabelSlotsMap_insertNew(lst, label);
    if (! record)
        return SHAREMIND_NULL;

    SharemindAssemblerLabelSlots_init(&record->value);
    return &record->value;
}

SHAREMIND_STRINGMAP_DECLARE_foreach_detail(
    static inline bool SmAsLabelSlotsMap_slotsAreFilled,
    SmAsLabelSlotsMap,
    SHAREMIND_COMMA SharemindAssemblerLabelSlot ** p,)

SHAREMIND_STRINGMAP_DEFINE_foreach_detail(
    static inline bool SmAsLabelSlotsMap_slotsAreFilled,
    SmAsLabelSlotsMap,
    SHAREMIND_COMMA SharemindAssemblerLabelSlot ** p,,
    true,
    if (! SharemindAssemblerLabelSlots_all_slots_filled(&v->value, p))
        return false;
    )

SHAREMIND_ENUM_CUSTOM_DEFINE_TOSTRING(SharemindAssemblerError,
                                      SHAREMIND_ASSEMBLER_ERROR_ENUM)

#define EOF_TEST     (unlikely(  t >= e))
#define INC_EOF_TEST (unlikely(++t >= e))

#define INC_CHECK_EOF(eof) \
    if (INC_EOF_TEST) { \
        goto eof; \
    } else (void) 0

#define DO_EOL(eof,noexpect) \
    do { \
        if (EOF_TEST) \
            goto eof; \
        if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_NEWLINE)) \
            goto noexpect; \
    } while ((0))

#define INC_DO_EOL(eof,noexpect) \
    do { \
        t++; \
        DO_EOL(eof,noexpect); \
    } while ((0))

SharemindAssemblerError sharemind_assembler_assemble(
        const SharemindAssemblerTokens * ts,
        SharemindAssemblerLinkingUnits * lus,
        const SharemindAssemblerToken ** errorToken,
        char ** errorString)
{
    SharemindAssemblerError returnStatus;
    SharemindAssemblerToken const * t;
    SharemindAssemblerToken const * e;
    SharemindAssemblerLinkingUnit * lu;
    uint8_t lu_index = 0u;
    int section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
    size_t numBindings = 0u;
    size_t numPdBindings = 0u;
    void * dataToWrite = SHAREMIND_NULL;
    size_t dataToWriteLength = 0u;

    /* for .data and .fill: */
    uint64_t multiplier;
    uint_fast8_t type;
    static const size_t widths[8] = { 1u, 2u, 4u, 8u, 1u, 2u, 4u, 8u };

    assert(ts);
    assert(lus);
    assert(lus->size == 0u);

    if (errorToken)
        *errorToken = SHAREMIND_NULL;
    if (errorString)
        *errorString = SHAREMIND_NULL;

    SmAsLabelLocationMap ll;
    SmAsLabelLocationMap_init(&ll);

    SmAsLabelSlotsMap lst;
    SmAsLabelSlotsMap_init(&lst);

    {
        SmAsLabelLocationMap_value * l =
                SmAsLabelLocationMap_insertNew(&ll, "RODATA");
        if (unlikely(!l))
            goto assemble_out_of_memory;
        /* l->value.linkingUnit = SIZE_MAX; */ /* Not used. */
        l->value.section = -1;
        l->value.offset = 1u;
        l = SmAsLabelLocationMap_insertNew(&ll, "DATA");
        if (unlikely(!l))
            goto assemble_out_of_memory;
        /* l->value.linkingUnit = SIZE_MAX; */ /* Not used. */
        l->value.section = -1;
        l->value.offset = 2u;
        l = SmAsLabelLocationMap_insertNew(&ll, "BSS");
        if (unlikely(!l))
            goto assemble_out_of_memory;
        /* l->value.linkingUnit = SIZE_MAX; */ /* Not used. */
        l->value.section = -1;
        l->value.offset = 3u;
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

            void * const hint = SmAsLabelLocationMap_insertHint(&ll, label);

            /* Check for duplicate label: */
            if (unlikely(!hint)) {
                free(label);
                goto assemble_duplicate_label_t;
            }

            SmAsLabelLocationMap_value * const l =
                SmAsLabelLocationMap_insertAtHint(&ll, label, hint);
            if (unlikely(!l)) {
                free(label);
                goto assemble_out_of_memory;
            }

            if (section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND) {
                l->value.offset = numBindings;
            } else if (section_index
                       == SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND)
            {
                l->value.offset = numPdBindings;
            } else {
                l->value.offset = lu->sections[section_index].length;
            }
            l->value.section = section_index;
            l->value.linkingUnit = lu_index;

            /* Fill pending label slots: */
            SmAsLabelSlotsMap_value * const record =
                SmAsLabelSlotsMap_get(&lst, label);
            free(label);
            if (record && ! SharemindAssemblerLabelSlots_fillSlots(
                        &record->value, &l->value))
                goto assemble_invalid_label_t;

            break;
        }
        case SHAREMIND_ASSEMBLER_TOKEN_DIRECTIVE:
#define TOKEN_MATCH(name) \
    ((t->length == sizeof(name) - 1u) \
     && strncmp(t->text, name, sizeof(name) - 1u) == 0)
            if (TOKEN_MATCH(".linking_unit")) {
                INC_CHECK_EOF(assemble_unexpected_eof);
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
                        lu = &lus->data[v];
                    }
                    lu_index = (uint8_t) v;
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
                }
            } else if (TOKEN_MATCH(".section")) {
                INC_CHECK_EOF(assemble_unexpected_eof);
                if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_KEYWORD))
                    goto assemble_invalid_parameter_t;

                if (TOKEN_MATCH("TEXT")) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
                } else if (TOKEN_MATCH("RODATA")) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_RODATA;
                } else if (TOKEN_MATCH("DATA")) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_DATA;
                } else if (TOKEN_MATCH("BSS")) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS;
                } else if (TOKEN_MATCH("BIND")) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND;
                } else if (TOKEN_MATCH("PDBIND")) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND;
                } else if (TOKEN_MATCH("DEBUG")) {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_DEBUG;
                } else {
                    goto assemble_invalid_parameter_t;
                }
            } else if (TOKEN_MATCH(".data")) {
                if (unlikely(section_index
                             == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT))
                    goto assemble_unexpected_token_t;

                multiplier = 1u;
                goto assemble_data_or_fill;
            } else if (TOKEN_MATCH(".fill")) {
                if (unlikely((section_index
                              == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT)
                             || (section_index
                                 == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND)
                             || (section_index
                                 == SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND)))
                    goto assemble_unexpected_token_t;

                INC_CHECK_EOF(assemble_unexpected_eof);

                if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_UHEX))
                    goto assemble_invalid_parameter_t;

                multiplier = SharemindAssemblerToken_uhex_value(t);
                if (unlikely(multiplier >= 65536u))
                    goto assemble_invalid_parameter_t;

                goto assemble_data_or_fill;
            } else if (likely(TOKEN_MATCH(".bind"))) {
                if (unlikely((section_index
                              != SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND)
                             && (section_index
                                 != SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND)))
                    goto assemble_unexpected_token_t;

                INC_CHECK_EOF(assemble_unexpected_eof);

                if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_STRING))
                    goto assemble_invalid_parameter_t;

                size_t syscallSigLen;
                char * const syscallSig =
                        SharemindAssemblerToken_string_value(t, &syscallSigLen);
                if (!syscallSig)
                    goto assemble_out_of_memory;

                const size_t oldLen = lu->sections[section_index].length;
                const size_t newLen = oldLen + syscallSigLen + 1;
                void * newData =
                        realloc(lu->sections[section_index].data, newLen);
                if (unlikely(!newData)) {
                    free(syscallSig);
                    goto assemble_out_of_memory;
                }
                lu->sections[section_index].data = newData;
                lu->sections[section_index].length = newLen;

                memcpy(((char *) lu->sections[section_index].data) + oldLen,
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

            INC_DO_EOL(assemble_check_labels, assemble_unexpected_token_t);
            goto assemble_newline;
        case SHAREMIND_ASSEMBLER_TOKEN_KEYWORD:
        {
            if (unlikely(section_index
                         != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT))
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
                if (INC_EOF_TEST)
                    break;
                if (t->type == SHAREMIND_ASSEMBLER_TOKEN_NEWLINE) {
                    break;
                } else if (t->type == SHAREMIND_ASSEMBLER_TOKEN_KEYWORD) {
                    size_t const newSize = l + t->length + 1u;
                    if (unlikely(newSize < l))
                        goto assemble_invalid_parameter_t;
                    char * const newName =
                            (char *) realloc(name,
                                             sizeof(char) * (newSize + 1u));
                    if (unlikely(!newName)) {
                        free(name);
                        goto assemble_out_of_memory;
                    }
                    name = newName;
                    name[l] = '_';
                    strncpy(name + l + 1u, t->text, t->length);
                    l = newSize;
                } else if (likely((t->type == SHAREMIND_ASSEMBLER_TOKEN_UHEX)
                                  || (t->type == SHAREMIND_ASSEMBLER_TOKEN_HEX)
                                  || (t->type
                                      == SHAREMIND_ASSEMBLER_TOKEN_LABEL)
                                  || (t->type
                                      == SHAREMIND_ASSEMBLER_TOKEN_LABEL_O)))
                {
                    args++;
                } else {
                    goto assemble_invalid_parameter_t;
                }
            }
            name[l] = '\0';

            /* Detect and check instruction: */
            const SharemindVmInstruction * const i =
                    sharemind_vm_instruction_from_name(name);
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
            bool doJumpLabel;
            {
                char c[sizeof(i->code)];
                memcpy(c, &(i->code), sizeof(i->code));
                if (c[0u] == 0x04     /* Check for jump namespace */
                    && c[2u] == 0x01) /* and imm first argument OLB */
                {
                    jmpOffset = lu->sections[section_index].length;
                    doJumpLabel = true;
                } else {
                    jmpOffset = 0u;
                    doJumpLabel = false;
                }
            }

            /* Allocate whole instruction: */
            char * newData =
                    (char *) realloc(lu->sections[section_index].data,
                                     sizeof(SharemindCodeBlock)
                                     * (lu->sections[section_index].length
                                        + args + 1u));
            if (unlikely(!newData))
                goto assemble_out_of_memory;
            lu->sections[section_index].data = newData;
            SharemindCodeBlock * cbdata =
                    (SharemindCodeBlock *) lu->sections[section_index].data;
            SharemindCodeBlock * instr =
                    &cbdata[lu->sections[section_index].length];
            lu->sections[section_index].length += args + 1;

            /* Write instruction code */
            instr->uint64[0] = i->code;

            /* Write arguments: */
            for (;;) {
                if (++ot == t)
                    break;
                if (ot->type == SHAREMIND_ASSEMBLER_TOKEN_UHEX) {
                    doJumpLabel = false; /* Past first argument */
                    instr++;
                    instr->uint64[0] = SharemindAssemblerToken_uhex_value(ot);
                } else if (ot->type == SHAREMIND_ASSEMBLER_TOKEN_HEX) {
                    doJumpLabel = false; /* Past first argument */
                    instr++;
                    instr->int64[0] = SharemindAssemblerToken_hex_value(ot);
                } else if (likely((ot->type == SHAREMIND_ASSEMBLER_TOKEN_LABEL)
                                  || (ot->type
                                      == SHAREMIND_ASSEMBLER_TOKEN_LABEL_O)))
                {
                    instr++;
                    char * const label =
                            SharemindAssemblerToken_label_to_new_string(ot);
                    if (unlikely(!label))
                        goto assemble_out_of_memory;

                    /* Check whether label is defined: */
                    const SmAsLabelLocationMap_value * const record =
                        SmAsLabelLocationMap_get(&ll, label);

                    if (record) {
                        free(label);

                        const SharemindAssemblerLabelLocation * const loc =
                            &record->value;

                        /* Is this a jump instruction location? */
                        if (doJumpLabel) {
                            if ((loc->section
                                    != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT)
                                || (loc->linkingUnit != lu_index))
                            {
                                if (errorToken)
                                    *errorToken = ot;
                                goto assemble_invalid_label;
                            }

                            /* Because the label was defined & we're one-pass:*/
                            assert(jmpOffset >= loc->offset);

                            size_t absTarget = loc->offset;
                            if (!assign_add_sizet_int64(
                                    &absTarget,
                                    SharemindAssemblerToken_label_offset(ot))
                                || !substract_2sizet_to_int64(&instr->int64[0],
                                                              absTarget,
                                                              jmpOffset))
                            {
                                if (errorToken)
                                    *errorToken = ot;
                                goto assemble_invalid_label_offset;
                            }
                            /** \todo Maybe check whether there's really an
                                      instruction there. */
                        } else {
                            size_t absTarget = loc->offset;
                            int64_t const offset =
                                    SharemindAssemblerToken_label_offset(ot);
                            if (loc->section < 0) {
                                if (offset != 0)
                                    goto assemble_invalid_label_offset;
                            } else {
                                if (!assign_add_sizet_int64(&absTarget, offset))
                                {
                                    if (errorToken)
                                        *errorToken = ot;
                                    goto assemble_invalid_label_offset;
                                }
                            }
                            instr->uint64[0] = absTarget;
                        }
                    } else {
                        SharemindAssemblerLabelSlots * const slots =
                            SmAsLabelSlotsMap_getOrInsertNew(&lst, label);
                        free(label);
                        if (unlikely(!slots))
                            goto assemble_out_of_memory;

                        SharemindAssemblerLabelSlot * const slot =
                                SharemindAssemblerLabelSlots_push(slots);
                        if (unlikely(!slot))
                            goto assemble_out_of_memory;

                        /* Signal a relative jump label: */
                        SharemindCodeBlock * const cbData =
                                (SharemindCodeBlock *)
                                    lu->sections[section_index].data;

                        assert(instr > cbData);
                        assert(((uintmax_t) (instr - cbData)) <= SIZE_MAX);

                        slot->extraOffset =
                                SharemindAssemblerToken_label_offset(ot);
                        slot->jmpOffset = jmpOffset;
                        slot->data = &lu->sections[section_index].data;
                        slot->cbdata_index = (size_t) (instr - cbData);
                        slot->token = ot;
                        slot->section = section_index;
                        slot->doJumpLabel = doJumpLabel;
                        slot->linkingUnit = lu_index;
                    }
                    doJumpLabel = false; /* Past first argument */
                } else {
                    /* Skip keywords, because they're already included in the
                       instruction code. */
                    assert(ot->type == SHAREMIND_ASSEMBLER_TOKEN_KEYWORD);
                }
            }

            DO_EOL(assemble_check_labels, assemble_unexpected_token_t);
            goto assemble_newline;
        }
        case SHAREMIND_ASSEMBLER_TOKEN_HEX:
        case SHAREMIND_ASSEMBLER_TOKEN_UHEX:
        case SHAREMIND_ASSEMBLER_TOKEN_STRING:
        case SHAREMIND_ASSEMBLER_TOKEN_LABEL_O:
            goto assemble_unexpected_token_t;
        #ifdef __clang__
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wcovered-switch-default"
        #endif
        default: SHAREMIND_ABORT("lAa %d\n", (int) t->type);
        #ifdef __clang__
        #pragma GCC diagnostic pop
        #endif
    } /* switch */

    if (!INC_EOF_TEST)
        goto assemble_newline;

assemble_check_labels:

    /* Check for undefined labels: */
    {
        SharemindAssemblerLabelSlot * undefinedSlot;
        if (likely(SmAsLabelSlotsMap_slotsAreFilled(
                       &lst, &undefinedSlot)))
            goto assemble_ok;

        assert(undefinedSlot);
        if (errorToken)
            *errorToken = undefinedSlot->token;
        goto assemble_undefined_label;
    }

assemble_data_or_fill:

    INC_CHECK_EOF(assemble_unexpected_eof);

    if (unlikely(t->type != SHAREMIND_ASSEMBLER_TOKEN_KEYWORD))
        goto assemble_invalid_parameter_t;

    if (TOKEN_MATCH("uint8")) {
        type = 0u;
    } else if (TOKEN_MATCH("uint16")) {
        type = 1u;
    } else if (TOKEN_MATCH("uint32")) {
        type = 2u;
    } else if (TOKEN_MATCH("uint64")) {
        type = 3u;
    } else if (TOKEN_MATCH("int8")) {
        type = 4u;
    } else if (TOKEN_MATCH("int16")) {
        type = 5u;
    } else if (TOKEN_MATCH("int32")) {
        type = 6u;
    } else if (TOKEN_MATCH("int64")) {
        type = 7u;
    } else if (TOKEN_MATCH("string")) {
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

    INC_DO_EOL(assemble_data_write, assemble_data_opt_param);
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
        dataToWrite =
                SharemindAssemblerToken_string_value(t, &dataToWriteLength);
        if (!dataToWrite)
            goto assemble_out_of_memory;
    } else {
        goto assemble_invalid_parameter_t;
    }
    assert(dataToWrite);

    INC_DO_EOL(assemble_data_write, assemble_unexpected_token_t);

assemble_data_write:

    assert(section_index != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT);
    if (section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
        lu->sections[SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS].length +=
                (multiplier * dataToWriteLength);
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
    dataToWrite = SHAREMIND_NULL;
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
    SmAsLabelSlotsMap_destroy(&lst);
    SmAsLabelLocationMap_destroy(&ll);
    return returnStatus;
}
