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
#include <new>
#include <sharemind/codeblock.h>
#include <sharemind/Concat.h>
#include <sharemind/IntegralComparisons.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <sharemind/MakeUnique.h>
#include <sharemind/SimpleUnorderedStringMap.h>
#include <sstream>
#include <tuple>
#include <utility>


namespace sharemind {
namespace Assembler {
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
    static constexpr auto const int64max =
            std::numeric_limits<std::int64_t>::max();
    if (lhs >= rhs) {
        std::size_t const r = lhs - rhs;
        if (r > static_cast<std::uint64_t>(int64max))
            return false;
        (*dest) = static_cast<std::int64_t>(r);
    } else {
        std::size_t const mr = rhs - lhs;
        assert(mr > 0);
        static_assert(-std::numeric_limits<std::int64_t>::max()
                      == std::numeric_limits<std::int64_t>::min() + 1, "");
        static constexpr auto const absMin =
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()) + 1u;
        if (mr > absMin) {
            return false;
        } else if (mr == absMin) {
            (*dest) = std::numeric_limits<std::int64_t>::min();
        } else {
            (*dest) = -static_cast<std::int64_t>(mr);
        }
    }
    return true;
}


struct LabelLocation {

/* Methods: */

    LabelLocation(std::size_t offset_, int section_ = -1) noexcept
        : offset(offset_)
        , section(section_)
    {}

    LabelLocation(std::size_t offset_,
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

using LabelLocationMap = SimpleUnorderedStringMap<LabelLocation>;

struct LabelSlot {

    LabelSlot(std::int64_t extraOffset_,
              std::size_t jmpOffset_,
              CodeSection & codeSection_,
              std::size_t cbdata_index_,
              TokensVector::const_iterator tokenIt_,
              bool doJumpLabel_,
              std::uint8_t linkingUnit_)
        : extraOffset(extraOffset_)
        , jmpOffset(jmpOffset_)
        , codeSection(codeSection_)
        , cbdata_index(cbdata_index_)
        , tokenIt(tokenIt_)
        , doJumpLabel(doJumpLabel_)
        , linkingUnit(linkingUnit_)
    {}

    std::int64_t extraOffset;
    std::size_t jmpOffset;
    CodeSection & codeSection;
    std::size_t cbdata_index;
    TokensVector::const_iterator tokenIt;
    bool doJumpLabel;
    std::uint8_t linkingUnit;
};

struct LabelSlotsVector: public std::vector<LabelSlot> {

/* Methods: */

    using std::vector<LabelSlot>::vector;
    using std::vector<LabelSlot>::operator=;

    bool fillSlots(LabelLocation const & l,
                   TokensVector::const_iterator const endIt)
            noexcept
    {
        for (auto & value : *this) {
            assert(value.tokenIt != endIt);

            std::size_t absTarget = l.offset;
            if (!assign_add_sizet_int64(&absTarget, value.extraOffset))
                return false; /**< \todo Provide better diagnostics */

            SharemindCodeBlock toWrite;
            if (!value.doJumpLabel) { /* Normal absolute label */
                toWrite.uint64[0u] = absTarget;
            } else { /* Relative jump label */
                if (value.linkingUnit != l.linkingUnit)
                    return false; /**< \todo Provide better diagnostics */

                assert(value.jmpOffset < l.offset); /* Because we're one-pass. */

                if (!substract_2sizet_to_int64(&toWrite.int64[0u],
                                               absTarget,
                                               value.jmpOffset))
                    return false; /**< \todo Provide better diagnostics */
                /** \todo Maybe check whether there's really an instruction there */
            }
            value.codeSection[value.cbdata_index] = toWrite;
            value.tokenIt = endIt;
        }
        return true;
    }

};

struct LabelSlotsMap: public SimpleUnorderedStringMap<LabelSlotsVector> {

/* Methods: */

    using SimpleUnorderedStringMap<LabelSlotsVector>::SimpleUnorderedStringMap;
    using SimpleUnorderedStringMap<LabelSlotsVector>::operator=;

};

void dataSectionCreateOrAddData(std::unique_ptr<Section> & sectionPtr,
                                void const * const data,
                                std::size_t const dataSize,
                                std::size_t const multiplier = 1u)
{
    if (!sectionPtr) {
        sectionPtr = makeUnique<DataSection>(data, dataSize);
    } else {
        auto & ds = *static_cast<DataSection *>(sectionPtr.get());
        for (std::size_t i = 0u; i < multiplier; ++i)
            ds.addData(data, dataSize);
    }
}

} // anonymous namespace

#define EOF_TEST     (unlikely(  t >= e))
#define INC_EOF_TEST (unlikely(++t >= e))

#define INC_CHECK_EOF \
    if (INC_EOF_TEST) { \
        throw AssembleException(ts.end(), "Unexpected end-of-file!"); \
    } else (void) 0

#define DO_EOL(eof,noexpect) \
    do { \
        if (EOF_TEST) \
            goto eof; \
        if (unlikely(t->type() != Token::Type::NEWLINE)) \
            goto noexpect; \
    } while ((0))

#define INC_DO_EOL(eof,noexpect) \
    do { \
        t++; \
        DO_EOL(eof,noexpect); \
    } while ((0))

LinkingUnitsVector assemble(TokensVector const & ts) {
    if (ts.empty())
        throw AssembleException(ts.end(),
                                "Won't assemble an empty tokens vector!");

    TokensVector::const_iterator t(ts.begin());
    TokensVector::const_iterator const e(ts.end());
    LinkingUnit * lu;
    std::uint8_t lu_index = 0u;
    int section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
    std::size_t numBindings = 0u;
    std::size_t numPdBindings = 0u;
    std::size_t dataToWriteLength = 0u;

    /* for .data and .fill: */
    std::uint64_t multiplier;
    std::uint_fast8_t type;
    static std::size_t const widths[8] = { 1u, 2u, 4u, 8u, 1u, 2u, 4u, 8u };

    LabelLocationMap ll;
    ll.emplace("RODATA", 1u);
    ll.emplace("DATA", 2u);
    ll.emplace("BSS", 3u);

    LabelSlotsMap lst;

    LinkingUnitsVector lus;
    if (unlikely(ts.empty()))
        return lus;

    lus.emplace_back();
    lu = &lus.back();


assemble_newline:
    switch (t->type()) {
        case Token::Type::NEWLINE:
            break;
        case Token::Type::LABEL:
        {
            auto & sectionPtr = lu->sections[section_index];
            auto const r(
                    ll.emplace(
                        std::piecewise_construct,
                        std::make_tuple(t->labelValue()),
                        std::make_tuple(
                            (section_index
                             == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND)
                            ? numBindings
                            : ((section_index
                                == SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND)
                               ? numPdBindings
                               : sectionPtr
                                 ? ((section_index
                                     == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT)
                                    ? static_cast<CodeSection *>(
                                          sectionPtr.get())->numInstructions()
                                    : sectionPtr->numBytes())
                                 : 0u),
                            section_index,
                            lu_index)));
            if (!r.second)
                throw AssembleException(t, concat("Duplicate label: \"",
                                                  t->labelValue(), '"'));

            /* Fill pending label slots: */
            auto const recordIt(lst.find(r.first->first));
            if (recordIt != lst.end()) {
                if (!recordIt->second.fillSlots(r.first->second, ts.end()))
                    throw AssembleException(t, concat("Invalid label: \"",
                                                      t->labelValue(), '"'));
                lst.erase(recordIt);
            }
            break;
        }
        case Token::Type::DIRECTIVE:
            if (t->directiveValue() == "linking_unit") {
                INC_CHECK_EOF;
                if (unlikely(t->type() != Token::Type::UHEX))
                    goto assemble_invalid_parameter_t;

                auto const v = t->uhexValue();
                if (unlikely(v > std::numeric_limits<std::uint8_t>::max()))
                    goto assemble_invalid_parameter_t;

                if (likely(v != lu_index)) {
                    if (unlikely(v > lus.size()))
                        goto assemble_invalid_parameter_t;
                    if (v == lus.size()) {
                        lus.emplace_back();
                        lu = &lus.back();
                    } else {
                        lu = &lus[v];
                    }
                    lu_index = static_cast<std::uint8_t>(v);
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
                }
            } else if (t->directiveValue() == "section") {
                INC_CHECK_EOF;
                if (unlikely(t->type() != Token::Type::KEYWORD))
                    goto assemble_invalid_parameter_t;

                if (t->keywordValue() == "TEXT") {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT;
                } else if (t->keywordValue() == "RODATA") {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_RODATA;
                } else if (t->keywordValue() == "DATA") {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_DATA;
                } else if (t->keywordValue() == "BSS") {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS;
                } else if (t->keywordValue() == "BIND") {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND;
                } else if (t->keywordValue() == "PDBIND") {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND;
                } else if (t->keywordValue() == "DEBUG") {
                    section_index = SHAREMIND_EXECUTABLE_SECTION_TYPE_DEBUG;
                } else {
                    goto assemble_invalid_parameter_t;
                }
            } else if (t->directiveValue() == "data") {
                if (unlikely((section_index
                              == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT)
                             || (section_index
                                 == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND)
                             || (section_index
                                 == SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND)))
                    goto assemble_unexpected_token_t;

                multiplier = 1u;
                goto assemble_data_or_fill;
            } else if (t->directiveValue() == "fill") {
                if (unlikely((section_index
                              == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT)
                             || (section_index
                                 == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND)
                             || (section_index
                                 == SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND)))
                    goto assemble_unexpected_token_t;

                INC_CHECK_EOF;

                if (unlikely(t->type() != Token::Type::UHEX))
                    goto assemble_invalid_parameter_t;

                multiplier = t->uhexValue();
                if (unlikely(multiplier >= 65536u))
                    goto assemble_invalid_parameter_t;

                goto assemble_data_or_fill;
            } else if (likely(t->directiveValue() == "bind")) {
                if (unlikely((section_index
                              != SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND)
                             && (section_index
                                 != SHAREMIND_EXECUTABLE_SECTION_TYPE_PDBIND)))
                    goto assemble_unexpected_token_t;

                INC_CHECK_EOF;

                if (unlikely(t->type() != Token::Type::STRING))
                    goto assemble_invalid_parameter_t;

                auto const syscallSig(t->stringValue());

                dataSectionCreateOrAddData(lu->sections[section_index],
                                           syscallSig.c_str(),
                                           syscallSig.size() + 1u);

                if (section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_BIND) {
                    numBindings++;
                } else {
                    numPdBindings++;
                }
            } else {
                throw AssembleException(t, concat("Unknown directive: .",
                                                  t->directiveValue()));
            }

            INC_DO_EOL(assemble_check_labels, assemble_unexpected_token_t);
            goto assemble_newline;
        case Token::Type::KEYWORD:
        {
            if (unlikely(section_index
                         != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT))
                goto assemble_unexpected_token_t;

            std::size_t args = 0u;
            std::string name = t->keywordValue();

            auto ot(t);
            /* Collect instruction name and count arguments: */
            for (;;) {
                if (INC_EOF_TEST)
                    break;
                if (t->type() == Token::Type::NEWLINE) {
                    break;
                } else if (t->type() == Token::Type::KEYWORD) {
                    name.push_back('_');
                    name.append(t->keywordValue());
                } else if (likely((t->type() == Token::Type::UHEX)
                                  || (t->type() == Token::Type::HEX)
                                  || (t->type() == Token::Type::LABEL)
                                  || (t->type() == Token::Type::LABEL_O)))
                {
                    args++;
                } else {
                    goto assemble_invalid_parameter_t;
                }
            }

            /* Detect and check instruction: */
            auto const & instrNameMap = instructionNameMap();
            auto const instrIt(instrNameMap.find(name));
            if (instrIt == instrNameMap.end()) {
                throw AssembleException(ot,
                                        concat("Unknown instruction: ", name));
            }
            auto const & i = instrIt->second;
            if (unlikely(i.numArgs != args))
                throw AssembleException(ot,
                                        concat("Instruction \"", name,
                                               "\" expects ", i.numArgs,
                                               " arguments, but only ", args,
                                               " given!"));

            // Create code section, if not yet created:
            auto & sectionPtr = lu->sections[section_index];
            if (!sectionPtr)
                sectionPtr = makeUnique<CodeSection>();
            auto & cs = *static_cast<CodeSection *>(sectionPtr.get());

            /* Detect offset for jump instructions */
            std::size_t jmpOffset;
            bool doJumpLabel;
            {
                char c[sizeof(i.code)];
                std::memcpy(c, &(i.code), sizeof(i.code));
                if (c[0u] == 0x04     /* Check for jump namespace */
                    && c[2u] == 0x01) /* and imm first argument OLB */
                {
                    jmpOffset = cs.numInstructions();
                    doJumpLabel = true;
                } else {
                    jmpOffset = 0u;
                    doJumpLabel = false;
                }
            }

            /* Reserve memory for whole instruction: */
            cs.reserveMore(args + 1u);

            /* Write instruction code */
            {
                SharemindCodeBlock toWrite;
                toWrite.uint64[0] = i.code;
                cs.addCode(&toWrite, 1u);
            }

            /* Write arguments: */
            for (;;) {
                if (++ot == t)
                    break;
                if (ot->type() == Token::Type::UHEX) {
                    doJumpLabel = false; /* Past first argument */
                    SharemindCodeBlock toWrite;
                    toWrite.uint64[0] = ot->uhexValue();
                    cs.addCode(&toWrite, 1u);
                } else if (ot->type() == Token::Type::HEX) {
                    doJumpLabel = false; /* Past first argument */
                    SharemindCodeBlock toWrite;
                    toWrite.int64[0] = ot->hexValue();
                    cs.addCode(&toWrite, 1u);
                } else if (likely((ot->type() == Token::Type::LABEL)
                                  || (ot->type()
                                      == Token::Type::LABEL_O)))
                {
                    auto label(ot->labelValue());
                    SharemindCodeBlock toWrite;

                    /* Check whether label is defined: */
                    auto const recordIt(ll.find(label));
                    if (recordIt != ll.end()) {
                        auto const & loc = recordIt->second;

                        /* Is this a jump instruction location? */
                        if (doJumpLabel) {
                            if ((loc.section
                                    != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT)
                                || (loc.linkingUnit != lu_index))
                                throw AssembleException(
                                        ot,
                                        concat("Invalid label: ", label));

                            /* Because the label was defined & we're one-pass:*/
                            assert(jmpOffset >= loc.offset);

                            std::size_t absTarget = loc.offset;
                            if (!assign_add_sizet_int64(&absTarget,
                                                        ot->labelOffset())
                                || !substract_2sizet_to_int64(&toWrite.int64[0],
                                                              absTarget,
                                                              jmpOffset))
                                throw AssembleException(
                                        ot,
                                        "Invalid label offset!");
                            /** \todo Maybe check whether there's really an
                                      instruction there. */
                        } else {
                            auto absTarget = loc.offset;
                            auto const offset = ot->labelOffset();
                            if (loc.section < 0) {
                                if (offset != 0)
                                    throw AssembleException(
                                            ot,
                                            "Invalid label offset!");
                            } else {
                                if (!assign_add_sizet_int64(&absTarget, offset))
                                    throw AssembleException(
                                            ot,
                                            "Invalid label offset!");
                            }
                            toWrite.uint64[0] = absTarget;
                        }
                        cs.addCode(&toWrite, 1u);
                    } else {
                        /* Signal a relative jump label: */
                        auto const offset = cs.numInstructions();
                        lst[std::move(label)].emplace_back(
                                    ot->labelOffset(),
                                    jmpOffset,
                                    cs,
                                    offset,
                                    ot,
                                    doJumpLabel,
                                    lu_index);

                        // Write dummy placeholder value:
                        SharemindCodeBlock toWrite;
                        cs.addCode(&toWrite, 1u);
                    }
                    doJumpLabel = false; /* Past first argument */
                } else {
                    /* Skip keywords, because they're already included in the
                       instruction code. */
                    assert(ot->type() == Token::Type::KEYWORD);
                }
            }

            DO_EOL(assemble_check_labels, assemble_unexpected_token_t);
            goto assemble_newline;
        }
        case Token::Type::HEX:
        case Token::Type::UHEX:
        case Token::Type::STRING:
        case Token::Type::LABEL_O:
            goto assemble_unexpected_token_t;
    } /* switch */

    if (!INC_EOF_TEST)
        goto assemble_newline;

assemble_check_labels:

    /* Check for undefined labels: */
    if (likely(lst.empty()))
        return lus;
    throw AssembleException(
            lst.begin()->second.begin()->tokenIt,
            concat("Undefined label: ", lst.begin()->first));

assemble_data_or_fill:

    INC_CHECK_EOF;

    if (unlikely(t->type() != Token::Type::KEYWORD))
        goto assemble_invalid_parameter_t;

    if (t->keywordValue() == "uint8") {
        type = 0u;
    } else if (t->keywordValue() == "uint16") {
        type = 1u;
    } else if (t->keywordValue() == "uint32") {
        type = 2u;
    } else if (t->keywordValue() == "uint64") {
        type = 3u;
    } else if (t->keywordValue() == "int8") {
        type = 4u;
    } else if (t->keywordValue() == "int16") {
        type = 5u;
    } else if (t->keywordValue() == "int32") {
        type = 6u;
    } else if (t->keywordValue() == "int64") {
        type = 7u;
    } else if (t->keywordValue() == "string") {
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

    INC_CHECK_EOF;

    {
        std::vector<char> dataToWrite;
        if (t->type() == Token::Type::UHEX) {
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
            if (section_index != SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
                dataToWrite.resize(dataToWriteLength);
                std::memcpy(dataToWrite.data(), &v, dataToWriteLength);
            }
        } else if (t->type() == Token::Type::HEX) {
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
            if (section_index != SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
                dataToWrite.resize(dataToWriteLength);
                std::memcpy(dataToWrite.data(), &v, dataToWriteLength);
            }
        } else if (t->type() == Token::Type::STRING && type == 8u) {
            auto const s(t->stringValue());
            dataToWriteLength = s.size();
            if (section_index != SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
                dataToWrite.resize(dataToWriteLength + 1u);
                std::memcpy(dataToWrite.data(),
                            s.c_str(),
                            dataToWriteLength + 1u);
            }
        } else {
            goto assemble_invalid_parameter_t;
        }
        assert(!dataToWrite.empty());

        INC_DO_EOL(assemble_data_write, assemble_unexpected_token_t);

assemble_data_write:

        assert(section_index != SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT);
        auto & sectionPtr = lu->sections[section_index];
        if (section_index == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
            if (!sectionPtr) {
                sectionPtr = makeUnique<BssSection>(multiplier,
                                                    dataToWriteLength);
            } else {
                static_cast<BssSection *>(sectionPtr.get())->addNumBytes(
                            multiplier * dataToWriteLength);
            }
        } else {
            /* Actually write the values. */
            assert(!dataToWrite.empty());
            dataSectionCreateOrAddData(sectionPtr,
                                       dataToWrite.data(),
                                       dataToWriteLength,
                                       multiplier);
        }
        goto assemble_newline;
    }

assemble_unexpected_token_t:

    throw AssembleException(t, concat("Unexpected token: ", *t));

assemble_invalid_parameter_t:

    throw AssembleException(t, "Invalid parameter!");

}

} // namespace Assembler {
} // namespace sharemind {
