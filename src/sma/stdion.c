#include "stdion.h"


int fnputs(const char * s, size_t len, FILE * stream) {
    int r = 0;
    for (; len; s++, len--) {
        r = fputc(*s, stream);
        if (r < 0)
            break;
    }
    return r;
}

int nputs(const char * s, size_t len) {
    int r = 0;
    for (; len; s++, len--) {
        r = putchar(*s);
        if (r < 0)
            break;
    }
    return r;
}
