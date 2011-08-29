#
# This file is a part of the Sharemind framework.
# Copyright (C) Cybernetica AS
#
# All rights are reserved. Reproduction in whole or part is prohibited
# without the written consent of the copyright owner. The usage of this
# code is subject to the appropriate license agreement.
#

include(../../vm.pri)

TEMPLATE = app
TARGET = smas
DESTDIR = ../../bin/

unix:!macx:LIBS += -Wl,-rpath-link=../../lib
LIBS += -L../../lib -lsmas

SOURCES += \
    main.c \
