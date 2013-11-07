#
# This file is a part of the Sharemind framework.
# Copyright (C) Cybernetica AS
#
# All rights are reserved. Reproduction in whole or part is prohibited
# without the written consent of the copyright owner. The usage of this
# code is subject to the appropriate license agreement.
#

CMAKE_MINIMUM_REQUIRED(VERSION "2.8.12")
CMAKE_POLICY(SET CMP0015 NEW)
PROJECT("SHAREMIND_LIBAS" "C")

FOREACH(dir IN LISTS CMAKE_PREFIX_PATH)
  INCLUDE_DIRECTORIES(BEFORE SYSTEM "${dir}/include")
ENDFOREACH()
INCLUDE_DIRECTORIES(BEFORE SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/../")

ADD_COMPILE_OPTIONS(
  "-O2"
  "$<$<NOT:$<STREQUAL:$<CONFIGURATION>,Release>>:-ggdb>"
  "$<$<NOT:$<STREQUAL:$<CONFIGURATION>,Release>>:-fno-omit-frame-pointer>"
  "$<$<STREQUAL:$<CONFIGURATION>,Release>:-DNDEBUG>"
  "$<$<STREQUAL:$<CONFIGURATION>,Release>:-fomit-frame-pointer>"
)

FUNCTION(SHAREMIND_FIND_TARGETLIB libname)
  IF(NOT DEFINED SHAREMIND_TARGETLIB_${libname})
    IF(TARGET "lib${libname}")
      SET(SHAREMIND_TARGETLIB_${libname} "lib${libname}" PARENT_SCOPE)
    ELSE()
      FIND_LIBRARY(libfn "sharemind_${libname}")
      IF(NOT libfn)
        IF("${ARGN}" STREQUAL "OPTIONAL")
          MESSAGE(WARNING "Library \"sharemind_${libname}\" not found!")
        ELSE()
          MESSAGE(FATAL_ERROR "Required library \"sharemind_${libname}\" not found!")
        ENDIF()
      ELSE()
        SET(libs)
        GET_FILENAME_COMPONENT(libpath "${libfn}" PATH)
        LIST(APPEND libs "-L${libpath}")
        LIST(APPEND libs "sharemind_${libname}")
        SET(SHAREMIND_TARGETLIB_${libname} "${libs}" PARENT_SCOPE)
      ENDIF()
    ENDIF()
  ENDIF()
ENDFUNCTION()

SHAREMIND_FIND_TARGETLIB("executable")
SHAREMIND_FIND_TARGETLIB("vmi")

FIND_PACKAGE(Boost REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})


SET(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -fstrict-aliasing -Wstrict-aliasing=1 -fwrapv -Wabi -Wbad-function-cast -Wc++-compat -Wformat -Wswitch-default -Wunused -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wconversion -Wsign-conversion -Wlogical-op -Wunused-macros")
SET(LIBAS_INSTALL_DEFINITIONS "-D__STDC_LIMIT_MACROS")
ADD_DEFINITIONS("${LIBAS_INSTALL_DEFINITIONS}")

################################################################################
#  Libas:                                                                      #
################################################################################

IF(NOT SHAREMIND_LIBAS_LINKING_MODE)
    SET(SHAREMIND_LIBAS_LINKING_MODE SHARED)
ENDIF()

ADD_LIBRARY("libas" ${SHAREMIND_LIBAS_LINKING_MODE}
            "libas/assemble.c"
            "libas/linker.c"
            "libas/linkingunits.c"
            "libas/stdion.c"
            "libas/tokenizer.c"
            "libas/tokens.c")

SET(LIBAS_VERSION "0.1.0.0")
SET_TARGET_PROPERTIES("libas" PROPERTIES
                      OUTPUT_NAME "sharemind_as"
                      LIBRARY_OUTPUT_DIRECTORY "${SHAREMIND_LIB_DIR}"
                      VERSION "${LIBAS_VERSION}"
                      SOVERSION "0.1")

TARGET_LINK_LIBRARIES("libas" ${SHAREMIND_TARGETLIB_executable}
                              ${SHAREMIND_TARGETLIB_vmi})

INSTALL(TARGETS "libas"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib")
INSTALL(DIRECTORY "libas/"
        DESTINATION "include/sharemind/libas"
        FILES_MATCHING PATTERN "stdion.h" EXCLUDE
                       PATTERN "*.h")

# Install cmake files:
FILE(GLOB_RECURSE sharemind_libas_in_files "${CMAKE_CURRENT_SOURCE_DIR}/*.in")
ADD_CUSTOM_TARGET("include_sharemind_libas_in_files_in_IDE" SOURCES ${sharemind_libas_in_files})
SET(LIBAS_INSTALL_LINK_LIBRARIES "-L${CMAKE_INSTALL_PREFIX}/lib" "-lsharemind_as")
SET(LIBAS_INSTALL_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibasConfig.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibasConfig.cmake" @ONLY)
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibasConfigVersion.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibasConfigVersion.cmake" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibasConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibasConfigVersion.cmake"
        DESTINATION "lib/SharemindLibas")
