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

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(Boost REQUIRED)
FIND_PACKAGE(SharemindLibexecutable REQUIRED)
FIND_PACKAGE(SharemindLibvmi REQUIRED)
FIND_PACKAGE(SharemindVmGlobals REQUIRED)

SET(LIBAS_EXTERNAL_INCLUDE_DIRS
    ${Boost_INCLUDE_DIRS}
    ${SharemindLibexecutable_INCLUDE_DIRS}
    ${SharemindLibvmi_INCLUDE_DIRS}
    ${SharemindVmGlobals_INCLUDE_DIRS}
)
LIST(REMOVE_DUPLICATES LIBAS_EXTERNAL_INCLUDE_DIRS)
INCLUDE_DIRECTORIES(${LIBAS_EXTERNAL_INCLUDE_DIRS})
SET(LIBAS_INSTALL_INCLUDE_DIRS ${LIBAS_EXTERNAL_INCLUDE_DIRS}
                               "${CMAKE_INSTALL_PREFIX}/include")
LIST(REMOVE_DUPLICATES LIBAS_INSTALL_INCLUDE_DIRS)

SET(LIBAS_EXTERNAL_LIBRARIES
    ${Boost_LIBRARIES}
    ${SharemindLibexecutable_LIBRARIES}
    ${SharemindLibvmi_LIBRARIES}
    ${SharemindVmGlobals_LIBRARIES}
)
LIST(REMOVE_DUPLICATES LIBAS_EXTERNAL_LIBRARIES)
SET(LIBAS_INSTALL_LINK_LIBRARIES ${LIBAS_EXTERNAL_LIBRARIES}
                                 "-L${CMAKE_INSTALL_PREFIX}/lib"
                                 "-lsharemind_as")
LIST(REMOVE_DUPLICATES LIBAS_INSTALL_LINK_LIBRARIES)

SET(LIBAS_EXTERNAL_DEFINITIONS
    ${Boost_DEFINITIONS}
    ${SharemindLibexecutable_DEFINITIONS}
    ${SharemindLibvmi_DEFINITIONS}
    ${SharemindVmGlobals_DEFINITIONS}
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

SET(LIBAS_VERSION "0.1.0.0")
SET_TARGET_PROPERTIES("libas" PROPERTIES
                      OUTPUT_NAME "sharemind_as"
                      LIBRARY_OUTPUT_DIRECTORY "${SHAREMIND_LIB_DIR}"
                      VERSION "${LIBAS_VERSION}"
                      SOVERSION "0.1")
TARGET_LINK_LIBRARIES("libas" ${LIBAS_EXTERNAL_LIBRARIES})

INSTALL(TARGETS "libas"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib")
INSTALL(FILES ${libas_HEADERS} DESTINATION "include/sharemind/libas")

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
        DESTINATION "lib/SharemindLibas")
