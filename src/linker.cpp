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

#include "linker.h"

#include <cassert>
#include <cstring>
#include <limits>
#include <sharemind/codeblock.h>
#include <sharemind/Concat.h>
#include <sharemind/libexecutable/libexecutable.h>
#include <sharemind/libexecutable/libexecutable_0x0.h>
#include <type_traits>


namespace sharemind {
namespace Assembler {

SHAREMIND_DEFINE_EXCEPTION_CONST_STDSTRING_NOINLINE(Exception,, LinkerException)

Section::~Section() noexcept { ::free(data); }

namespace {

inline std::size_t tryAddSizes(std::size_t const a, std::size_t const b) {
    if (std::numeric_limits<std::size_t>::max() - a < b)
        throw LinkerException("Resulting executable size too big!");
    return a + b;
}

inline std::size_t tryMulSizes(std::size_t const a, std::size_t const b) {
    if (std::numeric_limits<std::size_t>::max() / a < b)
        throw LinkerException("Resulting executable size too big!");
    return a * b;
}

std::size_t const extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

std::size_t size_0x0(LinkingUnitsVector const & lus) {
    assert(!lus.empty());
    if (lus.size() - 1u > std::numeric_limits<std::uint8_t>::max())
        throw LinkerException("Too many linking units given!");

    std::size_t r = sizeof(SharemindExecutableHeader0x0);
    std::size_t li = 0u;
    for (auto const & lu : lus) {
        r = tryAddSizes(r, sizeof(SharemindExecutableUnitHeader0x0));
        std::size_t si = 0u;
        for (auto const & section : lu.sections) {
            if (section.length > 0u
                && (section.data != nullptr
                    || si == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS))
            {
                r = tryAddSizes(r, sizeof(SharemindExecutableSectionHeader0x0));
                if (si == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT) {
                    if (section.length
                        > (std::numeric_limits<std::uint32_t>::max()
                           / sizeof(SharemindCodeBlock)))
                        throw LinkerException(
                                concat("Linking unit ", li, " section ", si,
                                       " (TEXT) too large!"));
                    r = tryAddSizes(
                            r,
                            tryMulSizes(section.length,
                                        sizeof(SharemindCodeBlock)));
                } else {
                    if (section.length
                        > std::numeric_limits<std::uint32_t>::max())
                        throw LinkerException(
                                concat("Linking unit ", li, " section ", si,
                                       " too large!"));
                    if (si != SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS)
                        r = tryAddSizes(
                                r,
                                tryAddSizes(section.length,
                                            extraPadding[section.length % 8]));
                }
            }
            ++si;
        }
        ++li;
    }
    return r;
}

char * writeSection_0x0(Section const & s,
                        char * p,
                        SHAREMIND_EXECUTABLE_SECTION_TYPE type)
{
    assert(s.length > 0u
           && (s.data || type == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS));

    /* Check for unsupported output format. */
    using U = std::underlying_type<SHAREMIND_EXECUTABLE_SECTION_TYPE>::type;
    if (type >= SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0)
        throw LinkerException(concat("Unsupported section type: ",
                                     static_cast<U>(type)));

    /* Write header: */
    if (s.length > std::numeric_limits<std::uint32_t>::max() / 8)
        throw LinkerException(concat("Section of size ", s.length,
                                     " too large!"));
    auto const l = static_cast<std::uint32_t>(s.length);

    {
        SharemindExecutableSectionHeader0x0 h;
        SharemindExecutableSectionHeader0x0_init(&h, type, l);
        std::memcpy(p, &h, sizeof(h));
        p += sizeof(h);
    }

    if (type == SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT) {
        /* Write section data */
        assert(s.length // Overflow check already done by size_0x0()
               <= (std::numeric_limits<std::uint32_t>::max()
                   / sizeof(SharemindCodeBlock)));
        auto const toWrite = s.length * sizeof(SharemindCodeBlock);
        std::memcpy(p, s.data, toWrite);
        p += toWrite;
    } else if (type != SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS) {
        /* Write section data */
        std::memcpy(p, s.data, l);
        p += l;

        /* Extra padding: */
        std::memset(p, '\0', extraPadding[l % 8]);
        p += extraPadding[l % 8];
    }
    return p;
}

std::vector<char> link_0x0(std::vector<char> data,
                           char * writePtr,
                           LinkingUnitsVector const & lus,
                           std::uint8_t activeLinkingUnit)
{
    assert(!lus.empty());
    assert(lus.size() - 1u <= UINT8_MAX);

    {
        SharemindExecutableHeader0x0 h;
        SharemindExecutableHeader0x0_init(
                    &h,
                    static_cast<std::uint8_t>(lus.size() - 1u),
                    activeLinkingUnit);
        std::memcpy(writePtr, &h, sizeof(h));
        writePtr += sizeof(h);
    }

    for (auto const & lu : lus) {
        /* Calculate number of sections: */
        std::uint8_t sections = 0u;
        for (std::size_t i = 0u; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0; ++i)
            if (lu.sections[i].length > 0u
                && (lu.sections[i].data
                    || i == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS))
                sections++;
        assert(sections > 0u);
        sections--;

        /* Write unit header */
        {
            SharemindExecutableUnitHeader0x0 h;
            SharemindExecutableUnitHeader0x0_init(&h, sections);
            std::memcpy(writePtr, &h, sizeof(h));
            writePtr += sizeof(h);
        }

        /* Write sections: */
        for (std::size_t i = 0u; i < SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT_0x0; ++i)
            if (lu.sections[i].length > 0u
                && (lu.sections[i].data
                    || i == SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS))
                writePtr =
                        writeSection_0x0(
                           lu.sections[i],
                           writePtr,
                           static_cast<SHAREMIND_EXECUTABLE_SECTION_TYPE>(i));
    }
    return data;
}

} // anonymous namespace

std::vector<char> link(std::uint16_t version,
                       LinkingUnitsVector const & lus,
                       std::uint8_t activeLinkingUnit)
{
    if (version > 0u)
        throw LinkerException(concat("Version unsupported by linker: ", version));

    if (lus.empty())
        throw LinkerException("No linking units defined!");

    static constexpr auto const headerSize =
            sizeof(SharemindExecutableCommonHeader);
    std::vector<char> data(tryAddSizes(headerSize, size_0x0(lus)));
    {
        SharemindExecutableCommonHeader h;
        SharemindExecutableCommonHeader_init(&h, version);
        std::memcpy(data.data(), &h, sizeof(h));
    }
    return link_0x0(std::move(data),
                    data.data() + headerSize,
                    lus,
                    activeLinkingUnit);
}

} /* namespace Assembler { */
} /* namespace sharemind { */
