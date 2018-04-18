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

#ifndef SHAREMIND_LIBAS_LINKER_H
#define SHAREMIND_LIBAS_LINKER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <sharemind/codeblock.h>
#include <sharemind/libexecutable/libexecutable_0x0.h>
#include <sharemind/ExceptionMacros.h>
#include <unordered_map>
#include <vector>
#include "Exception.h"


namespace sharemind {
namespace Assembler {

SHAREMIND_DECLARE_EXCEPTION_CONST_STDSTRING_NOINLINE(Exception,
                                                     LinkerException);

using SectionType = ExecutableSectionHeader0x0::SectionType;

struct Section {

    virtual ~Section() noexcept;

    virtual void const * bytes() const noexcept = 0;
    virtual std::size_t numBytes() const noexcept = 0;

};

class BssSection final: public Section {

public: /* Methods: */

    BssSection(std::size_t numElements,
               std::size_t elementSizeInBytes)
        : m_sizeInBytes(
            [numElements, elementSizeInBytes]() {
                if ((std::numeric_limits<std::size_t>::max() / numElements)
                    < elementSizeInBytes)
                    throw std::bad_array_new_length();
                return numElements * elementSizeInBytes;
            }())
    {}

    bool addNumBytes(std::size_t toAdd) noexcept {
        if (std::numeric_limits<std::size_t>::max() - toAdd < m_sizeInBytes)
            return false;
        m_sizeInBytes += toAdd;
        return true;
    }

    void const * bytes() const noexcept final override;
    std::size_t numBytes() const noexcept final override;

private: /* Fields: */

    std::size_t m_sizeInBytes;

};

class DataSection final: public Section {

public: /* Methods: */

    DataSection(void const * data, std::size_t size)
        : m_data(static_cast<char const *>(data),
                 static_cast<char const *>(data) + size)
    {}

    void addData(void const * data, std::size_t size) {
        m_data.insert(m_data.end(),
                      static_cast<char const *>(data),
                      static_cast<char const *>(data) + size);
    }

    void const * bytes() const noexcept final override;
    std::size_t numBytes() const noexcept final override;

private: /* Fields: */

    std::vector<char> m_data;

};

class CodeSection final: public Section {

public: /* Methods: */

    std::size_t numInstructions() const noexcept { return m_data.size(); }

    void addCode(SharemindCodeBlock const * data, std::size_t size)
    { m_data.insert(m_data.end(), data, data + size); }

    SharemindCodeBlock & operator[](std::size_t const i) noexcept {
        assert(i < m_data.size());
        return m_data[i];
    }

    void const * bytes() const noexcept final override;
    std::size_t numBytes() const noexcept final override;

private: /* Fields: */

    std::vector<SharemindCodeBlock> m_data;

};

struct LinkingUnit {
    std::unordered_map<SectionType, std::unique_ptr<Section> > sections{
                static_cast<std::underlying_type<SectionType>::type>(
                    SectionType::Count)};
};

using LinkingUnitsVector = std::vector<LinkingUnit>;

std::vector<char> link(std::uint16_t version,
                       LinkingUnitsVector const & lus,
                       std::uint8_t activeLinkingUnit = 0u);

} /* namespace Assembler { */
} /* namespace sharemind { */

#endif /* SHAREMIND_LIBAS_LINKER_H */
