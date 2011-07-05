include(../../vm.pri)

TEMPLATE = app
TARGET = sma
DESTDIR = ../../bin/

LIBS += -L../../lib -lsma

SOURCES += \
    main.c \
