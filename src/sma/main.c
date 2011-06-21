#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../trie.h"
#include "tokens.h"
#include "tokenizer.h"


SVM_TRIE_DECLARE(LabelLocations,size_t)
SVM_TRIE_DEFINE(LabelLocations,size_t,malloc,free)

int main() {

    const char * program =
        "nop\n"
        "resizestack 0x10\n"
        "mov_imm_reg 0x256 0x9\n"
        "mov_imm_stack 0x257 0x8\n"
        "jmp_imm 0x1\n"
        "nop\n"
        "nop\n"
        "push_imm 0x255\n"
        "push_reg 0x9\n"
        "push_stack 0x8\n"
        "halt 0x255\n"
        "nop\n";

    /* Tokenize: */

    size_t sl = 0u;
    size_t sc = 0u;
    struct Tokens * ts = tokenize(program, strlen(program), &sl, &sc);
    if (!ts)
        goto tokenize_fail;

    tokens_print(ts);

    /* PASS 1: Get label values */

    struct LabelLocations ll;
    LabelLocations_init(&ll);

    /*for (size_t i = 0; i < ts->numTokens; i++) {
        struct Token * t = &ts->array[i];
    }*/
    /** \todo PASS 1 */

    /* PASS 2: Assemble */
    /** \todo PASS 2 */

    free(ts);
    return EXIT_SUCCESS;

tokenize_fail:

    fprintf(stderr, "Tokenization failed at (%lu,%lu)!\n", sl, sc);
    return EXIT_FAILURE;

}
