#
# This file is a part of the Sharemind framework.
# Copyright (C) Cybernetica AS
#
# All rights are reserved. Reproduction in whole or part is prohibited
# without the written consent of the copyright owner. The usage of this
# code is subject to the appropriate license agreement.
#

CMAKE_MINIMUM_REQUIRED(VERSION "2.8")
CMAKE_POLICY(SET CMP0015 NEW)
PROJECT("SHAREMIND_LIBAS" "C")

FIND_PACKAGE(Boost REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})


SET(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -O2 -fstrict-aliasing -Wstrict-aliasing=1 -fwrapv -Wabi -Wbad-function-cast -Wc++-compat -Wformat -Wswitch-default -Wunused -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wconversion -Wsign-conversion -Wlogical-op -Wunused-macros")
ADD_DEFINITIONS("-D_XOPEN_SOURCE=500" "-D__STDC_LIMIT_MACROS")

################################################################################
#  Libas:                                                                      #
################################################################################

ADD_LIBRARY("libas" SHARED
            "libas/assemble.c"
            "libas/linker.c"
            "libas/linkingunits.c"
            "libas/stdion.c"
            "libas/tokenizer.c"
            "libas/tokens.c")
SET_TARGET_PROPERTIES("libas" PROPERTIES
                      OUTPUT_NAME "sharemind_as"
                      LIBRARY_OUTPUT_DIRECTORY "${SHAREMIND_LIB_DIR}"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1")

TARGET_LINK_LIBRARIES("libas" "libvmi" "libexecutable")
INSTALL(TARGETS "libas"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib")
INSTALL(DIRECTORY "libas/"
        DESTINATION "${SHAREMIND_INCLUDE_DESTINATION}/libas"
        FILES_MATCHING PATTERN "stdion.h" EXCLUDE
                       PATTERN "*.h")
