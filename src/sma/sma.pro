include(../../vm.pri)

TEMPLATE = app
TARGET = ../../bin/sma

SOURCES += \
    ../vm/instr.c \
    main.c \
    stdion.c \
    tokenizer.c \
    tokens.c

QMAKE_CFLAGS += -fstrict-aliasing -Wstrict-aliasing=1
