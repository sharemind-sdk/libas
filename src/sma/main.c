#include <stdio.h>
#include <stdlib.h>
#include "../libsma/assemble.h"
#include "../libsma/linker.h"
#include "../libsma/tokens.h"
#include "../libsma/tokenizer.h"


int main() {

    const char * program =
        ":start nop\n"
        "resizestack 0x10\n"
        ":test mov imm 0x256 reg 0x9\n"
        "mov imm 0x257 stack 0x8\n"
        ":here jmp imm :here+0x3\n"
        "nop\n"
        "nop\n"
        "push imm 0x255\n"
        "push reg 0x9\n"
        "push stack 0x8\n"
        "halt 0x255\n"/*
        "jmp imm :cae\n"
        "jmp imm :cae2b\n"*/
        "nop\n";

    /* Tokenize: */
    struct SMA_Tokens * ts;
    {
        size_t sl = 0u;
        size_t sc = 0u;
        ts = SMA_tokenize(program, strlen(program), &sl, &sc);
        if (unlikely(!ts)) {
            fprintf(stderr, "Tokenization failed at (%lu,%lu)!\n", sl, sc);
            return EXIT_FAILURE;
        }
        /* SMA_tokens_print(ts); */
    }

    /* Assemble the linking units: */
    struct SMA_LinkingUnits lus;
    {
        SMA_LinkingUnits_init(&lus);
        enum SMA_Assemble_Error r = SMA_assemble(ts, &lus);
        if (r != SMA_ASSEMBLE_OK) {
            SMA_LinkingUnits_destroy_with(&lus, &SMA_LinkingUnit_destroy);
            SMA_tokens_free(ts);

            fprintf(stderr, "Pass 1 failed with %s!\n", SMA_Assemble_Error_toString(r));
            return EXIT_FAILURE;
        }
        SMA_tokens_free(ts);
    }

    /* Generate the Sharemind Executable */
    size_t outputLength;
    char * output = SMA_link(0x0, &lus, &outputLength, 0);
    SMA_LinkingUnits_destroy_with(&lus, &SMA_LinkingUnit_destroy);
    if (!output) {
        fprintf(stderr, "Output failure!\n");
        return EXIT_FAILURE;
    }
    fwrite(output, 1, outputLength, stdout);
    free(output);

    return EXIT_SUCCESS;

}
