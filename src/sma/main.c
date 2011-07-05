#include <stdio.h>
#include <stdlib.h>
#include "assemble.h"
#include "tokens.h"
#include "tokenizer.h"


int main() {

    const char * program =
        ":start nop\n"
        "resizestack 0x10\n"
        ":test mov imm 0x256 reg 0x9\n"
        "mov imm 0x257 stack 0x8\n"
        "jmp imm 0x1\n"
        "nop\n"
        "nop\n"
        "push imm 0x255\n"
        "push reg 0x9\n"
        "push stack 0x8\n"
        "halt 0x255\n"
        "jmp imm :cae\n"
        "jmp imm :cae2b\n"
        "nop\n";

    /* Tokenize: */

    size_t sl = 0u;
    size_t sc = 0u;
    struct Tokens * ts = tokenize(program, strlen(program), &sl, &sc);
    if (!ts)
        goto main_tokenize_fail;

    tokens_print(ts);

    /* PASS 1: Get label values */

    struct LinkingUnits lus;
    LinkingUnits_init(&lus);

    enum SMA_Assemble_Error r = SMA_assemble(ts, &lus);
    if (r != SMA_ASSEMBLE_OK)
        goto main_pass_one_fail;

    /* PASS 2: Assemble */
    /** \todo PASS 2 */

    LinkingUnits_destroy_with(&lus, &LinkingUnit_destroy);
    tokens_free(ts);

    return EXIT_SUCCESS;

main_pass_one_fail:

    LinkingUnits_destroy_with(&lus, &LinkingUnit_destroy);
    tokens_free(ts);

    fprintf(stderr, "Pass 1 failed with %s!\n", SMA_Assemble_Error_toString(r));
    return EXIT_FAILURE;

main_tokenize_fail:

    fprintf(stderr, "Tokenization failed at (%lu,%lu)!\n", sl, sc);
    return EXIT_FAILURE;

}
