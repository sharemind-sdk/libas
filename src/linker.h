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
#include <sharemind/libexecutable/sharemind_executable_section_type.h>
#include <vector>
#include "Exception.h"


namespace sharemind {
namespace Assembler {

struct Section {
    ~Section() noexcept;
    std::size_t length = 0u;
    void * data = nullptr;
};


struct LinkingUnit {
    std::array<Section, SHAREMIND_EXECUTABLE_SECTION_TYPE_COUNT> sections;
};

using LinkingUnitsVector = std::vector<LinkingUnit>;

std::vector<char> link(std::uint16_t version,
                       LinkingUnitsVector const & lus,
                       std::uint8_t activeLinkingUnit = 0u);

} /* namespace Assembler { */
} /* namespace sharemind { */

#endif /* SHAREMIND_LIBAS_LINKER_H */
