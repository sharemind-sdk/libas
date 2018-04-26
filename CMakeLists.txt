#
# Copyright (C) Cybernetica
#
# Research/Commercial License Usage
# Licensees holding a valid Research License or Commercial License
# for the Software may use this file according to the written
# agreement between you and Cybernetica.
#
# GNU General Public License Usage
# Alternatively, this file may be used under the terms of the GNU
# General Public License version 3.0 as published by the Free Software
# Foundation and appearing in the file LICENSE.GPL included in the
# packaging of this file.  Please review the following information to
# ensure the GNU General Public License version 3.0 requirements will be
# met: http://www.gnu.org/copyleft/gpl-3.0.html.
#
# For further information, please contact us at sharemind@cyber.ee.
#

CMAKE_MINIMUM_REQUIRED(VERSION "3.0")
PROJECT(SharemindLibAs VERSION "0.3.0" LANGUAGES "CXX")

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(SharemindCMakeHelpers REQUIRED)

SharemindSetProjectVersion()


FIND_PACKAGE(Boost REQUIRED)
FIND_PACKAGE(SharemindCHeaders REQUIRED)
FIND_PACKAGE(SharemindCxxHeaders REQUIRED)
FIND_PACKAGE(SharemindLibExecutable 0.2.0 REQUIRED)
FIND_PACKAGE(SharemindLibVmi 0.2.0 REQUIRED)


# Headers:
FILE(GLOB_RECURSE SharemindLibAs_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
INSTALL(FILES ${SharemindLibAs_HEADERS}
        DESTINATION "include/sharemind/libas"
        COMPONENT "dev")


# The library:
SharemindSetCxx11CompileOptions(COMPILE_FLAGS "-fwrapv")
SharemindNewUniqueList(LIBAS_EXTERNAL_INCLUDE_DIRS
    ${Boost_INCLUDE_DIRS}
    ${SharemindCHeaders_INCLUDE_DIRS}
    ${SharemindCxxHeaders_INCLUDE_DIRS}
    ${SharemindLibExecutable_INCLUDE_DIRS}
    ${SharemindLibVmi_INCLUDE_DIRS}
)
SharemindNewUniqueList(LIBAS_EXTERNAL_LIBRARIES
    ${Boost_LIBRARIES}
    ${SharemindCHeaders_LIBRARIES}
    ${SharemindCxxHeaders_LIBRARIES}
    ${SharemindLibExecutable_LIBRARIES}
    ${SharemindLibVmi_LIBRARIES}
)
SharemindNewUniqueList(LIBAS_EXTERNAL_DEFINITIONS
    ${Boost_DEFINITIONS}
    ${SharemindCHeaders_DEFINITIONS}
    ${SharemindCxxHeaders_DEFINITIONS}
    ${SharemindLibExecutable_DEFINITIONS}
    ${SharemindLibVmi_DEFINITIONS}
)
FILE(GLOB_RECURSE SharemindLibAs_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
SharemindAddSharedLibrary("libas"
    OUTPUT_NAME "sharemind_as"
    SOURCES
        ${SharemindLibAs_SOURCES}
        ${SharemindLibAs_HEADERS}
    INCLUDE_DIRECTORIES
        ${LIBAS_EXTERNAL_INCLUDE_DIRS}
    LEGACY_DEFINITIONS
        ${LIBAS_EXTERNAL_DEFINITIONS}
    LINK_LIBRARIES
        ${LIBAS_EXTERNAL_LIBRARIES}
)


# Install cmake files:
SharemindCreateCMakeFindFiles(
    INCLUDE_DIRS
        "${CMAKE_INSTALL_PREFIX}/include"
        ${LIBAS_EXTERNAL_INCLUDE_DIRS}
    DEFINITIONS
        ${LIBAS_EXTERNAL_DEFINITIONS}
        "-D__STDC_LIMIT_MACROS"
    LIBRARIES
        ${LIBAS_EXTERNAL_LIBRARIES}
        "-L${CMAKE_INSTALL_PREFIX}/lib"
        "-lsharemind_as"
)


# Misc. files
FILE(GLOB MD_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.md")
FILE(GLOB_RECURSE DOC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/*")
FILE(GLOB LICENSE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE*")
ADD_CUSTOM_TARGET("include_MISC_FILES_in_IDE" SOURCES
    ${MD_FILES} ${DOC_FILES} ${LICENSE_FILES}
)


# Packaging
SharemindSetupPackaging()
SharemindAddComponentPackage("lib"
    NAME "libsharemind-as"
    DESCRIPTION "Sharemind Assembler library"
    DEB_SECTION "libs"
    DEB_DEPENDS
        "libsharemind-executable (>= 0.2.0)"
        "libsharemind-vmi (>= 0.2.0)"
        "libc6 (>= 2.19)"
)
SharemindAddComponentPackage("dev"
    NAME "libsharemind-as-dev"
    DESCRIPTION "Sharemind Assembler library development headers"
    DEB_SECTION "libdevel"
    DEB_DEPENDS
        "libsharemind-as (= ${SharemindLibAs_VERSION})"
        "libsharemind-executable-dev (>= 0.2.0)"
        "libsharemind-cheaders-dev (>= 0.1.0)"
        "libc6-dev (>= 2.19)"
)
SharemindAddComponentPackage("debug"
    NAME "libsharemind-as-dbg"
    DESCRIPTION "Sharemind Assembler library debug symbols"
    DEB_SECTION "debug"
    DEB_DEPENDS "libsharemind-as (= ${SharemindLibAs_VERSION})"
)
SharemindPackagingFinalize()
