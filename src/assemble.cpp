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
#include <sharemind/codeblock.h>
#include <sharemind/Concat.h>
#include <sharemind/IntegralComparisons.h>
#include <sharemind/libexecutable/libexecutable_0x0.h>
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

using SectionType = ExecutableSectionHeader0x0::SectionType;

struct LabelLocation {

/* Methods: */

    LabelLocation(std::size_t offset_,
                  SectionType section_ = SectionType::Invalid) noexcept
        : offset(offset_)
        , section(section_)
    {}

    LabelLocation(std::size_t offset_,
                  SectionType section_,
                  std::uint8_t linkingUnit_) noexcept
        : offset(offset_)
        , section(section_)
        , linkingUnit(linkingUnit_)
    {}

/* Fields: */

    std::size_t offset;
    SectionType section;
    std::uint8_t linkingUnit;

};

using LabelLocationMap = SimpleUnorderedStringMap<LabelLocation>;

struct LabelSlot {

    LabelSlot(std::int64_t extraOffset_,
              std::size_t jmpOffset_,
              decltype(Executable::TextSection::instructions) & codeSection_,
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
    decltype(Executable::TextSection::instructions) & codeSection;
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

class ResizableDataSection: public Executable::DataSection {

private: /* Types: */

    using Container = std::vector<char>;
    static_assert(std::is_same<Container::size_type, std::size_t>::value, "");

public: /* Methods: */

    ResizableDataSection()
        : ResizableDataSection(std::make_shared<Container>(), 0u)
    {}

    ResizableDataSection(void const * const data,
                         std::size_t const dataSize,
                         std::size_t multiplier = 1u)
        : ResizableDataSection(
            [](void const * data,
               std::size_t const dataSize,
               std::size_t multiplier)
            {
                if ((std::numeric_limits<std::size_t>::max() / multiplier)
                    < dataSize)
                    throw std::bad_array_new_length();
                auto r(std::make_shared<Container>(dataSize * multiplier));
                writeData(r->data(), data, dataSize, multiplier);
                return r;
            }(data, dataSize, multiplier),
            dataSize)
    {}

    void addData(void const * const data,
                 std::size_t const dataSize,
                 std::size_t multiplier = 1u)
    {
        assert(this->data);
        assert(this->data.get() == m_container.data());

        if (std::numeric_limits<std::size_t>::max() / multiplier < dataSize)
            throw std::bad_array_new_length();
        auto const toAdd = dataSize * multiplier;
        auto const oldSize = m_container.size();
        if ((std::numeric_limits<std::size_t>::max() - oldSize) < toAdd)
            throw std::bad_array_new_length();
        auto const totalDataSize = oldSize + toAdd;
        m_container.resize(totalDataSize);
        writeData(m_container.data() + oldSize, data, dataSize, multiplier);
        this->data = std::shared_ptr<void>(this->data, m_container.data());
        this->sizeInBytes = totalDataSize;
    }

private: /* Methods: */

    ResizableDataSection(std::shared_ptr<Container> containerPtr,
                         std::size_t const dataSize)
        : Executable::DataSection(
              std::shared_ptr<void>(containerPtr, containerPtr->data()),
              dataSize)
        , m_container(*containerPtr)
    {}

    static void writeData(char * writePtr,
                          void const * const data,
                          std::size_t const dataSize,
                          std::size_t multiplier)
    {
        for (;;) {
            std::memcpy(writePtr, data, dataSize);
            if (!--multiplier)
                break;
            writePtr += dataSize;
        }
    }

private: /* Fields: */

    Container & m_container;

};

void dataSectionCreateOrAddData(
        std::shared_ptr<Executable::DataSection> & sectionPtr,
        void const * const data,
        std::size_t const dataSize,
        std::size_t multiplier = 1u)
{
    if (!dataSize || !multiplier)
        return;

    assert(data);

    if (!sectionPtr) {
        sectionPtr = std::make_shared<ResizableDataSection>(data,
                                                            dataSize,
                                                            multiplier);
    } else {
        auto & resizeableSection =
                *static_cast<ResizableDataSection *>(sectionPtr.get());
        resizeableSection.addData(data, dataSize, multiplier);
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

Executable assemble(TokensVector const & ts) {
    if (ts.empty())
        throw AssembleException(ts.end(),
                                "Won't assemble an empty tokens vector!");

    TokensVector::const_iterator t(ts.begin());
    TokensVector::const_iterator const e(ts.end());
    std::uint8_t lu_index = 0u;
    auto sectionType = SectionType::Text;
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

    Executable exe;
    if (unlikely(ts.empty()))
        return exe;

    auto & lus = exe.linkingUnits;
    lus.emplace_back();
    auto lu = &lus.back();


assemble_newline:
    switch (t->type()) {
        case Token::Type::NEWLINE:
            break;
        case Token::Type::LABEL:
        {
            auto const registerLabel =
                    [&ll, &lst, &ts, t, sectionType, lu_index](
                            std::size_t offset)
                    {
                        auto const r(
                                ll.emplace(
                                    std::piecewise_construct,
                                    std::make_tuple(t->labelValue()),
                                    std::make_tuple(
                                            LabelLocation(offset,
                                                          sectionType,
                                                          lu_index))));
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
                    };

            switch (sectionType) {
                case SectionType::Text:
                    registerLabel(lu->textSection
                                  ? lu->textSection->instructions.size()
                                  : 0u);
                    break;
                case SectionType::RoData:
                    registerLabel(lu->roDataSection
                                  ? lu->roDataSection->sizeInBytes
                                  : 0u);
                    break;
                case SectionType::Data:
                    registerLabel(lu->rwDataSection
                                  ? lu->rwDataSection->sizeInBytes
                                  : 0u);
                    break;
                case SectionType::Bss:
                    registerLabel(lu->bssSection
                                  ? lu->bssSection->sizeInBytes
                                  : 0u);
                    break;
                case SectionType::Bind:
                    registerLabel(lu->bindingsSection
                                  ? lu->bindingsSection->bindings.size()
                                  : 0u);
                    break;
                case SectionType::PdBind:
                    registerLabel(lu->pdBindingsSection
                                  ? lu->pdBindingsSection->pdBindings.size()
                                  : 0u);
                    break;
                default:
                    assert(sectionType == SectionType::Debug);
                    registerLabel(lu->debugSection
                                  ? lu->debugSection->sizeInBytes
                                  : 0u);
                    break;
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
                    sectionType = SectionType::Text;
                }
            } else if (t->directiveValue() == "section") {
                INC_CHECK_EOF;
                if (unlikely(t->type() != Token::Type::KEYWORD))
                    goto assemble_invalid_parameter_t;

                if (t->keywordValue() == "TEXT") {
                    sectionType = SectionType::Text;
                } else if (t->keywordValue() == "RODATA") {
                    sectionType = SectionType::RoData;
                } else if (t->keywordValue() == "DATA") {
                    sectionType = SectionType::Data;
                } else if (t->keywordValue() == "BSS") {
                    sectionType = SectionType::Bss;
                } else if (t->keywordValue() == "BIND") {
                    sectionType = SectionType::Bind;
                } else if (t->keywordValue() == "PDBIND") {
                    sectionType = SectionType::PdBind;
                } else if (t->keywordValue() == "DEBUG") {
                    sectionType = SectionType::Debug;
                } else {
                    goto assemble_invalid_parameter_t;
                }
            } else if (t->directiveValue() == "data") {
                if (unlikely((sectionType == SectionType::Text)
                             || (sectionType == SectionType::Bind)
                             || (sectionType == SectionType::PdBind)))
                    goto assemble_unexpected_token_t;

                multiplier = 1u;
                goto assemble_data_or_fill;
            } else if (t->directiveValue() == "fill") {
                if (unlikely((sectionType == SectionType::Text)
                             || (sectionType == SectionType::Bind)
                             || (sectionType == SectionType::PdBind)))
                    goto assemble_unexpected_token_t;

                INC_CHECK_EOF;

                if (unlikely(t->type() != Token::Type::UHEX))
                    goto assemble_invalid_parameter_t;

                multiplier = t->uhexValue();
                if (unlikely(multiplier >= 65536u))
                    goto assemble_invalid_parameter_t;

                goto assemble_data_or_fill;
            } else if (likely(t->directiveValue() == "bind")) {
                if (unlikely((sectionType != SectionType::Bind)
                             && (sectionType != SectionType::PdBind)))
                    goto assemble_unexpected_token_t;

                INC_CHECK_EOF;

                if (unlikely(t->type() != Token::Type::STRING))
                    goto assemble_invalid_parameter_t;

                if (sectionType == SectionType::Bind) {
                    if (!lu->bindingsSection)
                        lu->bindingsSection =
                                std::make_shared<Executable::BindingsSection>();
                    lu->bindingsSection->bindings.emplace_back(
                                t->stringValue());
                } else {
                    assert(sectionType == SectionType::PdBind);
                    if (!lu->pdBindingsSection)
                        lu->pdBindingsSection =
                            std::make_shared<Executable::PdBindingsSection>();
                    lu->pdBindingsSection->pdBindings.emplace_back(
                                t->stringValue());
                }
            } else {
                throw AssembleException(t, concat("Unknown directive: .",
                                                  t->directiveValue()));
            }

            INC_DO_EOL(assemble_check_labels, assemble_unexpected_token_t);
            goto assemble_newline;
        case Token::Type::KEYWORD:
        {
            if (unlikely(sectionType != SectionType::Text))
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
            if (!lu->textSection)
                lu->textSection = std::make_shared<Executable::TextSection>();
            auto & csi = lu->textSection->instructions;
            auto const addCode =
                    [&csi](SharemindCodeBlock c)
                    { csi.emplace_back(std::move(c)); };

            /* Detect offset for jump instructions */
            std::size_t jmpOffset;
            bool doJumpLabel;
            {
                char c[sizeof(i.code)];
                std::memcpy(c, &(i.code), sizeof(i.code));
                if (c[0u] == 0x04     /* Check for jump namespace */
                    && c[2u] == 0x01) /* and imm first argument OLB */
                {
                    jmpOffset = csi.size();
                    doJumpLabel = true;
                } else {
                    jmpOffset = 0u;
                    doJumpLabel = false;
                }
            }

            /* Write instruction code */
            {
                SharemindCodeBlock toWrite;
                toWrite.uint64[0] = i.code;
                addCode(std::move(toWrite));
            }

            /* Write arguments: */
            for (;;) {
                if (++ot == t)
                    break;
                if (ot->type() == Token::Type::UHEX) {
                    doJumpLabel = false; /* Past first argument */
                    SharemindCodeBlock toWrite;
                    toWrite.uint64[0] = ot->uhexValue();
                    addCode(std::move(toWrite));
                } else if (ot->type() == Token::Type::HEX) {
                    doJumpLabel = false; /* Past first argument */
                    SharemindCodeBlock toWrite;
                    toWrite.int64[0] = ot->hexValue();
                    addCode(std::move(toWrite));
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
                            if ((loc.section != SectionType::Text)
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
                            if (loc.section == SectionType::Invalid) {
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
                    } else {
                        /* Signal a relative jump label: */
                        auto const offset = csi.size();
                        lst[std::move(label)].emplace_back(
                                    ot->labelOffset(),
                                    jmpOffset,
                                    csi,
                                    offset,
                                    ot,
                                    doJumpLabel,
                                    lu_index);

                        /* We still write a dummy placeholder value (from
                           variable toWrite) to the section and replace it later
                           once we have a value for it. We still need to
                           initialize it to silence valgrind: */
                        toWrite.uint64[0u] = 0u;
                    }
                    addCode(std::move(toWrite));
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
        return exe;
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
            if (sectionType != SectionType::Bss) {
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
            if (sectionType != SectionType::Bss) {
                dataToWrite.resize(dataToWriteLength);
                std::memcpy(dataToWrite.data(), &v, dataToWriteLength);
            }
        } else if (t->type() == Token::Type::STRING && type == 8u) {
            auto const s(t->stringValue());
            dataToWriteLength = s.size();
            if (sectionType != SectionType::Bss) {
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

        if (sectionType == SectionType::Bss) {
            if ((std::numeric_limits<std::size_t>::max() / multiplier)
                < dataToWriteLength)
                throw AssembleException(t, "BSS section grew too large!");
            if (!lu->bssSection) {
                lu->bssSection =
                        std::make_shared<Executable::BssSection>(
                            multiplier * dataToWriteLength);
            } else {
                auto const toAdd = multiplier * dataToWriteLength;
                auto const oldSizeInBytes = lu->bssSection->sizeInBytes;
                if ((std::numeric_limits<std::size_t>::max() - toAdd)
                    < oldSizeInBytes)
                    throw AssembleException(t, "BSS section grew too large!");
                lu->bssSection->sizeInBytes = oldSizeInBytes + toAdd;
            }
        } else {
            /* Actually write the values. */
            assert(!dataToWrite.empty());
            std::shared_ptr<Executable::DataSection> * sectionPtrPtr;
            switch (sectionType) {
            case SectionType::RoData:
                sectionPtrPtr = &lu->roDataSection;
                break;
            case SectionType::Data:
                sectionPtrPtr = &lu->rwDataSection;
                break;
            default:
                assert(sectionType == SectionType::Debug);
                sectionPtrPtr = &lu->debugSection;
                break;
            }
            dataSectionCreateOrAddData(*sectionPtrPtr,
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
