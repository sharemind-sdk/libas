#
# This file is a part of the Sharemind framework.
# Copyright (C) Cybernetica AS
#
# All rights are reserved. Reproduction in whole or part is prohibited
# without the written consent of the copyright owner. The usage of this
# code is subject to the appropriate license agreement.
#

include(../../vm.pri)

TEMPLATE = lib
TARGET = smas
DESTDIR = ../../lib/

SOURCES += \
    ../vm/instr.c \
    assemble.c \
    linker.c \
    linkingunits.c \
    stdion.c \
    tokenizer.c \
    tokens.c
