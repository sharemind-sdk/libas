#
# Copyright (C) 2015 Cybernetica
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

CMAKE_MINIMUM_REQUIRED(VERSION "2.8.12")
PROJECT("SHAREMIND_LIBAS" "C")

SET(CPACK_PACKAGE_VERSION_MAJOR "0")
SET(CPACK_PACKAGE_VERSION_MINOR "1")
SET(CPACK_PACKAGE_VERSION_PATCH "0")
SET(LIBAS_SOVERSION
    "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
SET(LIBAS_VERSION "${LIBAS_SOVERSION}.${CPACK_PACKAGE_VERSION_PATCH}")

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(Boost REQUIRED)
FIND_PACKAGE(SharemindCHeaders REQUIRED)
FIND_PACKAGE(SharemindLibexecutable REQUIRED)
FIND_PACKAGE(SharemindLibvmi REQUIRED)

SET(LIBAS_EXTERNAL_INCLUDE_DIRS
    ${Boost_INCLUDE_DIRS}
    ${SharemindCHeaders_INCLUDE_DIRS}
    ${SharemindLibexecutable_INCLUDE_DIRS}
    ${SharemindLibvmi_INCLUDE_DIRS}
)
LIST(REMOVE_DUPLICATES LIBAS_EXTERNAL_INCLUDE_DIRS)
INCLUDE_DIRECTORIES(${LIBAS_EXTERNAL_INCLUDE_DIRS})
SET(LIBAS_INSTALL_INCLUDE_DIRS ${LIBAS_EXTERNAL_INCLUDE_DIRS}
                               "${CMAKE_INSTALL_PREFIX}/include")
LIST(REMOVE_DUPLICATES LIBAS_INSTALL_INCLUDE_DIRS)

SET(LIBAS_EXTERNAL_LIBRARIES
    ${Boost_LIBRARIES}
    ${SharemindCHeaders_LIBRARIES}
    ${SharemindLibexecutable_LIBRARIES}
    ${SharemindLibvmi_LIBRARIES}
)
LIST(REMOVE_DUPLICATES LIBAS_EXTERNAL_LIBRARIES)
SET(LIBAS_INSTALL_LINK_LIBRARIES ${LIBAS_EXTERNAL_LIBRARIES}
                                 "-L${CMAKE_INSTALL_PREFIX}/lib"
                                 "-lsharemind_as")
LIST(REMOVE_DUPLICATES LIBAS_INSTALL_LINK_LIBRARIES)

SET(LIBAS_EXTERNAL_DEFINITIONS
    ${Boost_DEFINITIONS}
    ${SharemindCHeaders_DEFINITIONS}
    ${SharemindLibexecutable_DEFINITIONS}
    ${SharemindLibvmi_DEFINITIONS}
)
LIST(REMOVE_DUPLICATES LIBAS_EXTERNAL_DEFINITIONS)
SET(LIBAS_INSTALL_DEFINITIONS ${LIBAS_EXTERNAL_DEFINITIONS}
                              "-D__STDC_LIMIT_MACROS")
LIST(REMOVE_DUPLICATES LIBAS_INSTALL_DEFINITIONS)
ADD_DEFINITIONS(${LIBAS_INSTALL_DEFINITIONS})

ADD_COMPILE_OPTIONS(
  "-std=c99" "-O2" "-fwrapv" "-Wall" "-Wextra" "-Wunused" "-Wabi"
  "-Wstrict-aliasing=1" "-Wbad-function-cast" "-Wformat" "-Wswitch-default"
  "-Wfloat-equal" "-Wshadow" "-Wpointer-arith" "-Wcast-qual"
  "-Wstrict-prototypes" "-Wconversion" "-Wsign-conversion" "-Wunused-macros"
  "$<$<NOT:$<STREQUAL:$<CONFIGURATION>,Release>>:-ggdb>"
  "$<$<NOT:$<STREQUAL:$<CONFIGURATION>,Release>>:-fno-omit-frame-pointer>"
  "$<$<STREQUAL:$<CONFIGURATION>,Release>:-DNDEBUG>"
  "$<$<STREQUAL:$<CONFIGURATION>,Release>:-fomit-frame-pointer>"
)

INCLUDE(CheckCCompilerFlag)
FUNCTION(CHECK_ADD_COMPILER_FLAG flag)
    STRING(SUBSTRING "${flag}" 1 -1 FlagName)
    CHECK_C_COMPILER_FLAG("${flag}" C_COMPILER_HAS_${FlagName}_FLAG)
    IF(C_COMPILER_HAS_${FlagName}_FLAG)
        ADD_COMPILE_OPTIONS("${flag}")
    ENDIF()
ENDFUNCTION()
CHECK_ADD_COMPILER_FLAG("-Weverything")
CHECK_ADD_COMPILER_FLAG("-Wlogical-op")
CHECK_ADD_COMPILER_FLAG("-Wno-padded")
CHECK_ADD_COMPILER_FLAG("-Wno-gnu-case-range")

################################################################################
#  Libas:                                                                      #
################################################################################

IF(NOT SHAREMIND_LIBAS_LINKING_MODE)
    SET(SHAREMIND_LIBAS_LINKING_MODE SHARED)
ENDIF()

FILE(GLOB_RECURSE libas_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
LIST(REMOVE_ITEM libas_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/as.c")
FILE(GLOB_RECURSE libas_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
ADD_LIBRARY("libas" ${SHAREMIND_LIBAS_LINKING_MODE} ${libas_SOURCES} ${libas_HEADERS})
SET(libas_HEADERS_P "${CMAKE_CURRENT_SOURCE_DIR}/src/stdion.h")
FOREACH(h IN LISTS libas_HEADERS_P)
  LIST(REMOVE_ITEM libas_HEADERS "${h}")
ENDFOREACH()

SET_TARGET_PROPERTIES("libas" PROPERTIES
                      OUTPUT_NAME "sharemind_as"
                      LIBRARY_OUTPUT_DIRECTORY "${SHAREMIND_LIB_DIR}"
                      VERSION "${LIBAS_VERSION}"
                      SOVERSION "${LIBAS_SOVERSION}")
TARGET_LINK_LIBRARIES("libas" ${LIBAS_EXTERNAL_LIBRARIES})

INSTALL(TARGETS "libas"
        COMPONENT lib
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib")
INSTALL(FILES ${libas_HEADERS}
        DESTINATION "include/sharemind/libas"
        COMPONENT dev)

# Install cmake files:
FILE(GLOB_RECURSE sharemind_libas_in_files
                  "${CMAKE_CURRENT_SOURCE_DIR}/*.in")
ADD_CUSTOM_TARGET("include_sharemind_libas_in_files_in_IDE"
                  SOURCES ${sharemind_libas_in_files})

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibasConfig.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibasConfig.cmake" @ONLY)
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibasConfigVersion.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibasConfigVersion.cmake" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibasConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibasConfigVersion.cmake"
        DESTINATION "lib/SharemindLibas"
        COMPONENT dev)


################################################################################
# Misc. files
################################################################################

FILE(GLOB MD_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.md")
FILE(GLOB_RECURSE DOC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/*")
FILE(GLOB LICENSE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE*")
FILE(GLOB_RECURSE ALL_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
ADD_CUSTOM_TARGET("include_MISC_FILES_in_IDE" SOURCES
    ${MD_FILES} ${DOC_FILES} ${LICENSE_FILES} ${ALL_SOURCE_FILES}
)


################################################################################
# Packaging
################################################################################

SET(CPACK_PACKAGE_VENDOR "Cybernetica AS")
SET(CPACK_PACKAGE_CONTACT "Sharemind packaging <sharemind-packaging@cyber.ee>")
SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_NAME "libsharemind-as")
SET(CPACK_DEBIAN_PACKAGE_RELEASE "cyber1")
SET(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
SET(CPACK_DEB_COMPONENT_INSTALL "ON")
SET(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

SET(CPACK_DEBIAN_LIB_PACKAGE_NAME "libsharemind-as")
SET(CPACK_COMPONENT_DEV_DESCRIPTION
    "Sharemind Assembler library development headers")
SET(CPACK_COMPONENT_LIB_DESCRIPTION "Sharemind Assembler library")
SET(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "libsharemind-as (= ${LIBAS_VERSION}), libsharemind-executable-dev (>= 0.1.0), libsharemind-cheaders-dev (>= 0.1.0), libc6-dev (>= 2.19)")
SET(CPACK_DEBIAN_LIB_PACKAGE_DEPENDS "libsharemind-executable (>= 0.1.0), libsharemind-vmi (>= 0.1.0), libc6 (>= 2.19)")
SET(CPACK_DEBIAN_DEV_PACKAGE_SECTION "libdevel")
SET(CPACK_DEBIAN_LIB_PACKAGE_SECTION "libs")
INCLUDE(CPack)
