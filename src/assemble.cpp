/*
 * Copyright (C) Cybernetica
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
#include <limits>
#include <sharemind/abort.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <sharemind/SimpleUnorderedStringMap.h>
#include <tuple>
#include <utility>


namespace {

inline bool assign_add_sizet_int64(std::size_t * const lhs,
                                   std::int64_t const rhs)
{
    if (rhs > 0) {
        auto const urhs = static_cast<std::uint64_t>(rhs);
        if (urhs > std::numeric_limits<std::size_t>::max() - (*lhs))
            return false;
        (*lhs) += urhs;
    } else if (rhs < 0) {
        static_assert(-std::numeric_limits<std::int64_t>::max()
                      == std::numeric_limits<std::int64_t>::min() + 1, "");
        if (rhs == std::numeric_limits<std::int64_t>::min()) {
            static constexpr auto const absMin =
                    static_cast<std::uint64_t>(
                        std::numeric_limits<std::int64_t>::max()) + 1u;
            if ((*lhs) < absMin)
                return false;
            (*lhs) -= absMin;
        } else {
            auto const absRhs = static_cast<std::uint64_t>(-rhs);
            if ((*lhs) < absRhs)
                return false;
            (*lhs) -= absRhs;
        }
    }
    return true;
}

inline bool substract_2sizet_to_int64(std::int64_t * const dest,
                                      std::size_t const lhs,
                                      std::size_t const rhs)
{
    if (lhs >= rhs) {
        std::size_t r = lhs - rhs;
        if (r > std::numeric_limits<std::int64_t>::max())
            return false;
        (*dest) = (std::int64_t) r;
    } else {
        std::size_t mr = rhs - lhs;
        assert(mr > 0);
        if (mr - 1 > (std::uint64_t) std::numeric_limits<std::int64_t>::max()) {
            return false;
        } else if (mr - 1
                   == (std::uint64_t) std::numeric_limits<std::int64_t>::max())
        {
            (*dest) = std::numeric_limits<std::int64_t>::min();
        } else {
            (*dest) = -((std::int64_t) mr);
        }
    }
    return true;
}


struct SharemindAssemblerLabelLocation {

/* Methods: */

    SharemindAssemblerLabelLocation(std::size_t offset_,
                                    int section_ = -1) noexcept
        : offset(offset_)
        , section(section_)
    {}

    SharemindAssemblerLabelLocation(std::size_t offset_,
                                    int section_,
                                    std::uint8_t linkingUnit_) noexcept
        : offset(offset_)
        , section(section_)
        , linkingUnit(linkingUnit_)
    {}

/* Fields: */

    std::size_t offset;
    int section;
    std::uint8_t linkingUnit;

};

using SmAsLabelLocationMap =
        sharemind::SimpleUnorderedStringMap<SharemindAssemblerLabelLocation>;

struct SharemindAssemblerLabelSlot {

    SharemindAssemblerLabelSlot(
            std::int64_t extraOffset_,
            std::size_t jmpOffset_,
            void ** data_,
            std::size_t cbdata_index_,
            sharemind::AssemblerTokens::const_iterator tokenIt_,
            int section_,
            bool doJumpLabel_,
            std::uint8_t linkingUnit_)
        : extraOffset(extraOffset_)
        , jmpOffset(jmpOffset_)
        , data(data_)
        , cbdata_index(cbdata_index_)
        , tokenIt(tokenIt_)
        , section(section_)
        , doJumpLabel(doJumpLabel_)
        , linkingUnit(linkingUnit_)
    {}

    std::int64_t extraOffset;
    std::size_t jmpOffset;
    void ** data;
    std::size_t cbdata_index;
    sharemind::AssemblerTokens::const_iterator tokenIt;
    int section;
    bool doJumpLabel;
    std::uint8_t linkingUnit;
};

struct SharemindAssemblerLabelSlots
        : public std::vector<SharemindAssemblerLabelSlot>
{

/* Methods: */

    using std::vector<SharemindAssemblerLabelSlot>::vector;
    using std::vector<SharemindAssemblerLabelSlot>::operator=;

    bool fillSlots(SharemindAssemblerLabelLocation const & l,
                   sharemind::AssemblerTokens::const_iterator const endIt)
            noexcept
    {
        for (auto & value : *this) {
            assert(value.tokenIt != endIt);

            std::size_t absTarget = l.offset;
            if (!assign_add_sizet_int64(&absTarget, value.extraOffset))
                return false; /**< \todo Provide better diagnostics */

            if (!value.doJumpLabel) { /* Normal absolute label */
                ((SharemindCodeBlock *) *value.data)[value.cbdata_index].uint64[0]
                        = absTarget;
            } else { /* Relative jump label */
                if (value.section != l.section
                    || value.linkingUnit != l.linkingUnit)
                    return false; /**< \todo Provide better diagnostics */

                assert(value.section == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT);
                assert(value.jmpOffset < l.offset); /* Because we're one-pass. */

                if (!substract_2sizet_to_int64(
                        &((SharemindCodeBlock *) *value.data)[value.cbdata_index]
                                .int64[0],
                        absTarget,
                        value.jmpOffset))
                    return false; /**< \todo Provide better diagnostics */

                /** \todo Maybe check whether there's really an instruction there */
            }
            value.tokenIt = endIt;
        }
        return true;
    }

};

struct SmAsLabelSlotsMap
        : public sharemind::SimpleUnorderedStringMap<
                SharemindAssemblerLabelSlots>
{

/* Methods: */

    using sharemind::SimpleUnorderedStringMap<SharemindAssemblerLabelSlots>
            ::SimpleUnorderedStringMap;
    using sharemind::SimpleUnorderedStringMap<SharemindAssemblerLabelSlots>
            ::operator=;

};

} // anonymous namespace

SHAREMIND_ENUM_CUSTOM_DEFINE_TOSTRING(SharemindAssemblerError,
                                      SHAREMIND_ASSEMBLER_ERROR_ENUM)

#define EOF_TEST     (unlikely(  t >= e))
#define INC_EOF_TEST (unlikely(++t >= e))

#define INC_CHECK_EOF \
    if (INC_EOF_TEST) { \
        return SHAREMIND_ASSEMBLE_UNEXPECTED_EOF; \
    } else (void) 0

#define DO_EOL(eof,noexpect) \
    do { \
        if (EOF_TEST) \
            goto eof; \
        if (unlikely(t->type != AssemblerToken::Type::NEWLINE)) \
            goto noexpect; \
    } while ((0))

#define INC_DO_EOL(eof,noexpect) \
    do { \
        t++; \
        DO_EOL(eof,noexpect); \
    } while ((0))

SharemindAssemblerError sharemind_assembler_assemble(
        sharemind::AssemblerTokens const & ts,
        SharemindAssemblerLinkingUnits * lus,
        sharemind::AssemblerTokens::const_iterator * errorToken,
        char ** errorString)
{
    using sharemind::AssemblerToken;

    sharemind::AssemblerTokens::const_iterator t(ts.begin());
    sharemind::AssemblerTokens::const_iterator const e(ts.end());
    SharemindAssemblerLinkingUnit * lu;
    std::uint8_t lu_index = 0u;
    int section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
    std::size_t numBindings = 0u;
    std::size_t numPdBindings = 0u;
    void * dataToWrite = nullptr;
    std::size_t dataToWriteLength = 0u;

    /* for .data and .fill: */
    std::uint64_t multiplier;
    std::uint_fast8_t type;
    static std::size_t const widths[8] = { 1u, 2u, 4u, 8u, 1u, 2u, 4u, 8u };

    assert(lus);
    assert(lus->size == 0u);

    if (errorToken)
        *errorToken = ts.end();
    if (errorString)
        *errorString = nullptr;

    SmAsLabelLocationMap ll;
    ll.emplace("RODATA", 1u);
    ll.emplace("DATA", 2u);
    ll.emplace("BSS", 3u);

    SmAsLabelSlotsMap lst;

    lu = SharemindAssemblerLinkingUnits_push(lus);
    if (unlikely(!lu))
        return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;

    SharemindAssemblerLinkingUnit_init(lu);

    if (unlikely(ts.empty()))
        return SHAREMIND_ASSEMBLE_OK;

assemble_newline:
    switch (t->type) {
        case AssemblerToken::Type::NEWLINE:
            break;
        case AssemblerToken::Type::LABEL:
        {
            auto label(t->labelToString());

            auto const r(
                        ll.emplace(
                            std::piecewise_construct,
                            std::make_tuple(std::move(label)),
                            std::make_tuple(
                                (section_index
                                 == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND)
                                ? numBindings
                                : ((section_index
                                    == SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND)
                                   ? numPdBindings
                                   : lu->sections[section_index].length),
                                section_index,
                                lu_index)));
            if (!r.second) {
                if (errorToken)
                    *errorToken = t;
                return SHAREMIND_ASSEMBLE_DUPLICATE_LABEL;
            }

            /* Fill pending label slots: */
            auto const recordIt(lst.find(r.first->first));
            if (recordIt != lst.end()) {
                if (!recordIt->second.fillSlots(r.first->second, ts.end())) {
                    if (errorToken)
                        *errorToken = t;
                    return SHAREMIND_ASSEMBLE_INVALID_LABEL;
                }
                lst.erase(recordIt);
            }
            break;
        }
        case AssemblerToken::Type::DIRECTIVE:
#define TOKEN_MATCH(name) \
    ((t->length == sizeof(name) - 1u) \
     && strncmp(t->text, name, sizeof(name) - 1u) == 0)
            if (TOKEN_MATCH(".linking_unit")) {
                INC_CHECK_EOF;
                if (unlikely(t->type != AssemblerToken::Type::UHEX))
                    goto assemble_invalid_parameter_t;

                auto const v = t->uhexValue();
                if (unlikely(v > std::numeric_limits<std::uint8_t>::max()))
                    goto assemble_invalid_parameter_t;

                if (likely(v != lu_index)) {
                    if (unlikely(v > lus->size))
                        goto assemble_invalid_parameter_t;
                    if (v == lus->size) {
                        lu = SharemindAssemblerLinkingUnits_push(lus);
                        if (unlikely(!lu))
                            return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
                        SharemindAssemblerLinkingUnit_init(lu);
                    } else {
                        lu = &lus->data[v];
                    }
                    lu_index = (std::uint8_t) v;
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
                }
            } else if (TOKEN_MATCH(".section")) {
                INC_CHECK_EOF;
                if (unlikely(t->type != AssemblerToken::Type::KEYWORD))
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

                INC_CHECK_EOF;

                if (unlikely(t->type != AssemblerToken::Type::UHEX))
                    goto assemble_invalid_parameter_t;

                multiplier = t->uhexValue();
                if (unlikely(multiplier >= 65536u))
                    goto assemble_invalid_parameter_t;

                goto assemble_data_or_fill;
            } else if (likely(TOKEN_MATCH(".bind"))) {
                if (unlikely((section_index
                              != SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND)
                             && (section_index
                                 != SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND)))
                    goto assemble_unexpected_token_t;

                INC_CHECK_EOF;

                if (unlikely(t->type != AssemblerToken::Type::STRING))
                    goto assemble_invalid_parameter_t;

                auto const syscallSig(t->stringValue());

                std::size_t const oldLen = lu->sections[section_index].length;
                std::size_t const newLen = oldLen + syscallSig.size() + 1;
                void * newData =
                        realloc(lu->sections[section_index].data, newLen);
                if (unlikely(!newData))
                    return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
                lu->sections[section_index].data = newData;
                lu->sections[section_index].length = newLen;

                memcpy(((char *) lu->sections[section_index].data) + oldLen,
                       syscallSig.c_str(), syscallSig.size() + 1u);

                if (section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND) {
                    numBindings++;
                } else {
                    numPdBindings++;
                }
            } else {
                if (errorToken)
                    *errorToken = t;
                return SHAREMIND_ASSEMBLE_UNKNOWN_DIRECTIVE;
            }

            INC_DO_EOL(assemble_check_labels, assemble_unexpected_token_t);
            goto assemble_newline;
        case AssemblerToken::Type::KEYWORD:
        {
            if (unlikely(section_index
                         != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT))
                goto assemble_unexpected_token_t;

            std::size_t args = 0u;
            std::size_t l = t->length;
            char * name = (char *) malloc(sizeof(char) * (l + 1u));
            if (unlikely(!name))
                return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
            strncpy(name, t->text, l);

            auto ot(t);
            /* Collect instruction name and count arguments: */
            for (;;) {
                if (INC_EOF_TEST)
                    break;
                if (t->type == AssemblerToken::Type::NEWLINE) {
                    break;
                } else if (t->type == AssemblerToken::Type::KEYWORD) {
                    std::size_t const newSize = l + t->length + 1u;
                    if (unlikely(newSize < l))
                        goto assemble_invalid_parameter_t;
                    char * const newName =
                            (char *) realloc(name,
                                             sizeof(char) * (newSize + 1u));
                    if (unlikely(!newName)) {
                        free(name);
                        return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
                    }
                    name = newName;
                    name[l] = '_';
                    strncpy(name + l + 1u, t->text, t->length);
                    l = newSize;
                } else if (likely((t->type == AssemblerToken::Type::UHEX)
                                  || (t->type == AssemblerToken::Type::HEX)
                                  || (t->type
                                      == AssemblerToken::Type::LABEL)
                                  || (t->type
                                      == AssemblerToken::Type::LABEL_O)))
                {
                    args++;
                } else {
                    goto assemble_invalid_parameter_t;
                }
            }
            name[l] = '\0';

            /* Detect and check instruction: */
            SharemindVmInstruction const * const i =
                    sharemind_vm_instruction_from_name(name);
            if (unlikely(!i)) {
                if (errorToken)
                    *errorToken = ot;
                if (errorString)
                    *errorString = name;
                return SHAREMIND_ASSEMBLE_UNKNOWN_INSTRUCTION;
            }
            if (unlikely(i->numArgs != args)) {
                if (errorToken)
                    *errorToken = ot;
                if (errorString)
                    *errorString = name;
                return SHAREMIND_ASSEMBLE_INVALID_NUMBER_OF_PARAMETERS;
            }
            free(name);

            /* Detect offset for jump instructions */
            std::size_t jmpOffset;
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
                return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
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
                if (ot->type == AssemblerToken::Type::UHEX) {
                    doJumpLabel = false; /* Past first argument */
                    instr++;
                    instr->uint64[0] = ot->uhexValue();
                } else if (ot->type == AssemblerToken::Type::HEX) {
                    doJumpLabel = false; /* Past first argument */
                    instr++;
                    instr->int64[0] = ot->hexValue();
                } else if (likely((ot->type == AssemblerToken::Type::LABEL)
                                  || (ot->type
                                      == AssemblerToken::Type::LABEL_O)))
                {
                    instr++;
                    auto label(ot->labelToString());

                    /* Check whether label is defined: */
                    auto const recordIt(ll.find(label));
                    if (recordIt != ll.end()) {
                        auto const & loc = recordIt->second;

                        /* Is this a jump instruction location? */
                        if (doJumpLabel) {
                            if ((loc.section
                                    != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT)
                                || (loc.linkingUnit != lu_index))
                            {
                                if (errorToken)
                                    *errorToken = ot;
                                return SHAREMIND_ASSEMBLE_INVALID_LABEL;
                            }

                            /* Because the label was defined & we're one-pass:*/
                            assert(jmpOffset >= loc.offset);

                            std::size_t absTarget = loc.offset;
                            if (!assign_add_sizet_int64(&absTarget,
                                                        ot->labelOffset())
                                || !substract_2sizet_to_int64(&instr->int64[0],
                                                              absTarget,
                                                              jmpOffset))
                            {
                                if (errorToken)
                                    *errorToken = ot;
                                return SHAREMIND_ASSEMBLE_INVALID_LABEL_OFFSET;
                            }
                            /** \todo Maybe check whether there's really an
                                      instruction there. */
                        } else {
                            auto absTarget = loc.offset;
                            auto const offset = ot->labelOffset();
                            if (loc.section < 0) {
                                if (offset != 0)
                                    return SHAREMIND_ASSEMBLE_INVALID_LABEL_OFFSET;
                            } else {
                                if (!assign_add_sizet_int64(&absTarget, offset))
                                {
                                    if (errorToken)
                                        *errorToken = ot;
                                    return SHAREMIND_ASSEMBLE_INVALID_LABEL_OFFSET;
                                }
                            }
                            instr->uint64[0] = absTarget;
                        }
                    } else {
                        /* Signal a relative jump label: */
                        SharemindCodeBlock * const cbData =
                                (SharemindCodeBlock *)
                                    lu->sections[section_index].data;

                        assert(instr > cbData);
                        assert(((std::uintmax_t) (instr - cbData))
                               <= std::numeric_limits<std::size_t>::max());

                        lst[std::move(label)].emplace_back(
                                    ot->labelOffset(),
                                    jmpOffset,
                                    &lu->sections[section_index].data,
                                    static_cast<std::size_t>(instr - cbData),
                                    ot,
                                    section_index,
                                    doJumpLabel,
                                    lu_index);
                    }
                    doJumpLabel = false; /* Past first argument */
                } else {
                    /* Skip keywords, because they're already included in the
                       instruction code. */
                    assert(ot->type == AssemblerToken::Type::KEYWORD);
                }
            }

            DO_EOL(assemble_check_labels, assemble_unexpected_token_t);
            goto assemble_newline;
        }
        case AssemblerToken::Type::HEX:
        case AssemblerToken::Type::UHEX:
        case AssemblerToken::Type::STRING:
        case AssemblerToken::Type::LABEL_O:
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
    if (likely(lst.empty()))
        return SHAREMIND_ASSEMBLE_OK;
    if (errorToken)
        *errorToken = lst.begin()->second.begin()->tokenIt;
    return SHAREMIND_ASSEMBLE_UNDEFINED_LABEL;

assemble_data_or_fill:

    INC_CHECK_EOF;

    if (unlikely(t->type != AssemblerToken::Type::KEYWORD))
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
    if (t->type == AssemblerToken::Type::UHEX) {
        auto const v = t->uhexValue();
        switch (type) {
            case 0u: /* uint8 */
                if (v > std::numeric_limits<std::uint8_t>::max())
                    goto assemble_invalid_parameter_t;
                break;
            case 1u: /* uint16 */
                if (v > std::numeric_limits<std::uint16_t>::max())
                    goto assemble_invalid_parameter_t;
                break;
            case 2u: /* uint32 */
                if (v > std::numeric_limits<std::uint32_t>::max())
                    goto assemble_invalid_parameter_t;
                break;
            case 3u: /* uint64; All should be fine here. */
                break;
            case 4u: /* int8 */
                if (v > std::numeric_limits<std::int8_t>::max())
                    goto assemble_invalid_parameter_t;
                break;
            case 5u: /* int16 */
                if (v > std::numeric_limits<std::int16_t>::max())
                    goto assemble_invalid_parameter_t;
                break;
            case 6u: /* int32 */
                if (v > std::numeric_limits<std::int32_t>::max())
                    goto assemble_invalid_parameter_t;
                break;
            case 7u: /* int64 */
                if (v > std::numeric_limits<std::int64_t>::max())
                    goto assemble_invalid_parameter_t;
                break;
            case 8u: /* string */
                goto assemble_invalid_parameter_t;
            default:
                abort();
        }
        dataToWrite = malloc(dataToWriteLength);
        if (!dataToWrite)
            return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
        memcpy(dataToWrite, &v, dataToWriteLength);
    } else if (t->type == AssemblerToken::Type::HEX) {
        auto const v = t->hexValue();
        switch (type) {
            case 0u: /* uint8 */
                if (v > std::numeric_limits<std::uint8_t>::max() || v < 0)
                    goto assemble_invalid_parameter_t;
                break;
            case 1u: /* uint16 */
                if (v > std::numeric_limits<std::uint16_t>::max() || v < 0)
                    goto assemble_invalid_parameter_t;
                break;
            case 2u: /* uint32 */
                if (v > std::numeric_limits<std::uint32_t>::max() || v < 0)
                    goto assemble_invalid_parameter_t;
                break;
            case 3u: /* uint64 */
                if (v < 0)
                    goto assemble_invalid_parameter_t;
                break;
            case 4u: /* int8 */
                if (v < std::numeric_limits<std::int8_t>::min()
                    || v > std::numeric_limits<std::int8_t>::max())
                    goto assemble_invalid_parameter_t;
                break;
            case 5u: /* int16 */
                if (v < std::numeric_limits<std::int16_t>::min()
                    || v > std::numeric_limits<std::int16_t>::max())
                    goto assemble_invalid_parameter_t;
                break;
            case 6u: /* int32 */
                if (v < std::numeric_limits<std::int32_t>::min()
                    || v > std::numeric_limits<std::int32_t>::max())
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
            return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
        memcpy(dataToWrite, &v, dataToWriteLength);
    } else if (t->type == AssemblerToken::Type::STRING && type == 8u) {
        auto const s(t->stringValue());
        dataToWriteLength = s.size();
        dataToWrite = std::malloc(dataToWriteLength + 1u);
        if (!dataToWrite)
            return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
        std::memcpy(dataToWrite, s.c_str(), dataToWriteLength + 1u);
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
        std::size_t const oldLen = lu->sections[section_index].length;
        std::size_t const newLen = oldLen + (multiplier * dataToWriteLength);
        void * newData = realloc(lu->sections[section_index].data, newLen);
        if (unlikely(!newData)) {
            free(dataToWrite);
            return SHAREMIND_ASSEMBLE_OUT_OF_MEMORY;
        }
        lu->sections[section_index].data = newData;
        lu->sections[section_index].length = newLen;

        /* Actually write the values. */
        newData = ((std::uint8_t *) newData) + oldLen;
        if (dataToWrite) {
            for (;;) {
                memcpy(newData, dataToWrite, dataToWriteLength);
                if (!--multiplier)
                    break;
                newData = ((std::uint8_t *) newData) + dataToWriteLength;
            };
        } else {
            memset(newData, 0, dataToWriteLength);
        }
    }

    if (dataToWrite)
        free(dataToWrite);
    dataToWrite = nullptr;
    goto assemble_newline;

assemble_unexpected_token_t:
    if (errorToken)
        *errorToken = t;
    if (errorString) {
        *errorString = (char *) malloc(t->length + 1);
        strncpy(*errorString, t->text, t->length);
        *errorString[t->length] = '\0';
    }
    return SHAREMIND_ASSEMBLE_UNEXPECTED_TOKEN;

assemble_invalid_parameter_t:
    if (errorToken)
        *errorToken = t;
    return SHAREMIND_ASSEMBLE_INVALID_PARAMETER;
}
