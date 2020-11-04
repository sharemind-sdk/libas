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
PROJECT(SharemindLibAs VERSION "0.4.0" LANGUAGES "CXX")

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(SharemindCMakeHelpers 1.6 REQUIRED)

FIND_PACKAGE(SharemindCHeaders 1.3.0 REQUIRED)
FIND_PACKAGE(SharemindCxxHeaders 0.8.0 REQUIRED)
FIND_PACKAGE(SharemindLibExecutable 0.4.0 REQUIRED)
FIND_PACKAGE(SharemindLibVmi 0.4.0 REQUIRED)


# Headers:
FILE(GLOB_RECURSE SharemindLibAs_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
INSTALL(FILES ${SharemindLibAs_HEADERS}
        DESTINATION "include/sharemind/libas"
        COMPONENT "dev")


# The library:
SharemindSetCxx14CompileOptions()
FILE(GLOB_RECURSE SharemindLibAs_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
SharemindAddSharedLibrary(LibAs
    OUTPUT_NAME "sharemind_as"
    SOURCES
        ${SharemindLibAs_SOURCES}
        ${SharemindLibAs_HEADERS}
)
TARGET_INCLUDE_DIRECTORIES(LibAs
    INTERFACE
        # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> # TODO
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
    )
TARGET_COMPILE_DEFINITIONS(LibAs PUBLIC "__STDC_LIMIT_MACROS")
TARGET_COMPILE_OPTIONS(LibAs PRIVATE "-fwrapv")
TARGET_LINK_LIBRARIES(LibAs
    PRIVATE
        "Sharemind::LibVmi"
    PUBLIC
        "Sharemind::CHeaders"
        "Sharemind::CxxHeaders"
        "Sharemind::LibExecutable"
    )
SharemindCreateCMakeFindFilesForTarget(LibAs
    DEPENDENCIES
        "SharemindCHeaders 1.3.0"
        "SharemindCxxHeaders 0.8.0"
        "SharemindLibExecutable 0.4.0"
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
        "libstdc++6 (>= 4.8.0)"
        "libc6 (>= 2.19)"
)
SharemindAddComponentPackage("dev"
    NAME "libsharemind-as-dev"
    DESCRIPTION "Sharemind Assembler library development headers"
    DEB_SECTION "libdevel"
    DEB_DEPENDS
        "libsharemind-as (= ${SharemindLibAs_DEB_lib_PACKAGE_VERSION})"
        "libsharemind-executable-dev (>= 0.2.0)"
        "libsharemind-cheaders-dev (>= 1.1.0)"
        "libsharemind-cxxheaders-dev (>= 0.4.0)"
        "libstdc++-dev"
)
SharemindAddComponentPackage("debug"
    NAME "libsharemind-as-dbg"
    DESCRIPTION "Sharemind Assembler library debug symbols"
    DEB_SECTION "debug"
    DEB_DEPENDS "libsharemind-as (= ${SharemindLibAs_DEB_lib_PACKAGE_VERSION})"
)
SharemindPackagingFinalize()
