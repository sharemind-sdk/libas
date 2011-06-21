include(../../vm.pri)

TEMPLATE = app
TARGET = ../../bin/sma

SOURCES += \
    main.c \
    stdion.c \
    tokenizer.c \
    tokens.c

QMAKE_CFLAGS += -fstrict-aliasing -Wstrict-aliasing=1
