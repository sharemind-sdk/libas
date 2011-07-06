include(../../vm.pri)

TEMPLATE = lib
TARGET = sma
DESTDIR = ../../lib/

SOURCES += \
    ../vm/instr.c \
    assemble.c \
    linker.c \
    linkingunits.c \
    stdion.c \
    tokenizer.c \
    tokens.c
