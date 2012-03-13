#
# This file is a part of the Sharemind framework.
# Copyright (C) Cybernetica AS
#
# All rights are reserved. Reproduction in whole or part is prohibited
# without the written consent of the copyright owner. The usage of this
# code is subject to the appropriate license agreement.
#

CMAKE_MINIMUM_REQUIRED(VERSION "2.8")
PROJECT("SHAREMIND_LIBAS" "C")

SET(SHAREMIND_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")
SET(SHAREMIND_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib")
SET(SHAREMIND_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_sources")
SET(SHAREMIND_INCLUDE_DESTINATION "include/sharemind")
SET(SHAREMIND_EXT_DIR "${CMAKE_CURRENT_BINARY_DIR}/ext")
LINK_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}/lib/")

ENABLE_TESTING()

ADD_SUBDIRECTORY("src/sharemind/")
