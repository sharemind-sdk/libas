#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../libsma/assemble.h"
#include "../libsma/linker.h"
#include "../libsma/tokens.h"
#include "../libsma/tokenizer.h"
#include "../static_assert.h"


SVM_STATIC_ASSERT(sizeof(off_t) <= sizeof(size_t));

int main(int argc, char * argv[]) {
    const char * inName = NULL;
    const char * outName = NULL;

    /* Parse arguments */
    char activeOpt = '\0';
    for (int i = 1; i < argc; i++) {
        switch (activeOpt) {
            case 'o':
                if (outName != NULL) {
                    fprintf(stderr, "Error: Multiple output files specified!\n");
                    goto main_fail_1;
                } else {
                    outName = argv[i];
                }
                activeOpt = '\0';
                break;
            case '\0':
                if (argv[i][0] == '-') {
                    if (argv[i][1] == 'o' && argv[i][2] == '\0') {
                        activeOpt = 'o';
                    } else {
                        fprintf(stderr, "Error: Invalid argument: %s\n", argv[i]);
                        goto main_fail_1;
                    }
                } else {
                    if (inName != NULL) {
                        fprintf(stderr, "Error: Multiple input files specified!\n");
                        goto main_fail_1;
                    } else {
                        inName = argv[i];
                    }
                }
                break;
            default:
                abort();
                break;
        }
    }
    if (activeOpt != '\0') {
        fprintf(stderr, "Error: Argument expected!\n");
        goto main_fail_1;
    }
    if (inName == NULL) {
        fprintf(stderr, "Error: No input files specified!\n");
        goto main_fail_1;
    }
    if (outName == NULL) {
        fprintf(stderr, "Error: No output files specified!\n");
        goto main_fail_1;
    }

    /* Open input file: */
    int inFileD = open(inName, O_RDONLY);
    if (inFileD == -1) {
        fprintf(stderr, "Error opening file \"%s\" for reading!\n", inName);
        goto main_fail_1;
    }

    /* Determine input file size: */
    struct stat inFileStat;
    if (fstat(inFileD, &inFileStat) != 0) {
        fprintf(stderr, "Error: Failed to fstat input file \"%s\"!\n", inName);
        goto main_fail_2;
    }

    /* Memory map input file: */
    void * map = mmap(0, inFileStat.st_size, PROT_READ, MAP_SHARED, inFileD, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "Error: Failed to mmap the file \"%s\"!\n", inName);
        goto main_fail_2;
        return EXIT_FAILURE;
    }

    /* Tokenize: */
    struct SMA_Tokens * ts;
    {
        size_t sl = 0u;
        size_t sc = 0u;
        ts = SMA_tokenize(map, inFileStat.st_size, &sl, &sc);
        if (unlikely(!ts)) {
            fprintf(stderr, "Error: Tokenization failed at (%zu,%zu)!\n", sl, sc);
            goto main_fail_3;
        }
        /* SMA_tokens_print(ts); */
    }

    /* Assemble the linking units: */
    struct SMA_LinkingUnits lus;
    {
        SMA_LinkingUnits_init(&lus);
        enum SMA_Assemble_Error r = SMA_assemble(ts, &lus);
        if (r != SMA_ASSEMBLE_OK) {
            fprintf(stderr, "Error: Assembly failed with error %s!\n", SMA_Assemble_Error_toString(r));
            goto main_fail_4;
        }
        SMA_tokens_free(ts);

        if (munmap(map, inFileStat.st_size) != 0)
            fprintf(stderr, "Error: Failed to munmap input file!\n");
        if (close(inFileD))
            fprintf(stderr, "Error: Failed to close input file!\n");
    } /* Active resources: lus */

    /* Generate the Sharemind Executable: */
    size_t outputLength;
    char * output = SMA_link(0x0, &lus, &outputLength, 0);
    SMA_LinkingUnits_destroy_with(&lus, &SMA_LinkingUnit_destroy);
    if (!output) {
        fprintf(stderr, "Error generating output!\n");
        goto main_fail_1;
    }

    /* Open output file: */
    FILE * outFile = fopen(outName, "w");
    if (outFile == NULL) {
        fprintf(stderr, "Error opening output file \"%s\" for writing!\n", outName);
        goto main_fail_5;
    }

    /* Write output to output file: */
    if (fwrite(output, 1, outputLength, outFile) != outputLength) {
        fprintf(stderr, "Error writing output to file \"%s\"!\n", outName);
        goto main_fail_6;
    }

    /* Close output file: */
    if (fclose(outFile) != 0)
        fprintf(stderr, "Error closing output file \"%s\"!\n", outName);
    free(output);

    return EXIT_SUCCESS;

main_fail_6:

    if (fclose(outFile) != 0)
        fprintf(stderr, "Error closing output file \"%s\"!\n", outName);

main_fail_5:

    free(output);
    goto main_fail_1;

main_fail_4:

    SMA_LinkingUnits_destroy_with(&lus, &SMA_LinkingUnit_destroy);
    SMA_tokens_free(ts);

main_fail_3:

    if (munmap(map, inFileStat.st_size) != 0)
        fprintf(stderr, "Error: Failed to munmap input file!\n");

main_fail_2:

    if (close(inFileD))
        fprintf(stderr, "Error: Failed to close input file!\n");

main_fail_1:

    return EXIT_FAILURE;
}
