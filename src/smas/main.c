/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../libsmas/assemble.h"
#include "../libsmas/linker.h"
#include "../libsmas/tokenizer.h"
#include "../static_assert.h"


SM_STATIC_ASSERT(sizeof(off_t) <= sizeof(size_t));

int main(int argc, char * argv[]) {
    const char * inName = NULL;
    const char * outName = NULL;
    char activeOpt = '\0';
    int inFileD;
    struct stat inFileStat;
    size_t fileSize;
    void * map;
    SMAS_Tokens * ts;
    SMAS_LinkingUnits lus;
    size_t outputLength;
    uint8_t * output;
    FILE * outFile;

    /* Parse arguments */
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
    inFileD = open(inName, O_RDONLY);
    if (inFileD == -1) {
        fprintf(stderr, "Error opening file \"%s\" for reading!\n", inName);
        goto main_fail_1;
    }

    /* Determine input file size: */
    if (fstat(inFileD, &inFileStat) != 0) {
        fprintf(stderr, "Error: Failed to fstat input file \"%s\"!\n", inName);
        goto main_fail_2;
    }

    /* Memory map input file: */
    if (((uintmax_t) inFileStat.st_size) > SIZE_MAX) {
        fprintf(stderr, "Error: Input file \"%s\" too large!\n", inName);
        goto main_fail_2;
    }
    if (((uintmax_t) inFileStat.st_size) <= 0u) {
        fprintf(stderr, "Error: Input file \"%s\" is empty!\n", inName);
        goto main_fail_2;
    }
    fileSize = (size_t) inFileStat.st_size;

    map = mmap(0, fileSize, PROT_READ, MAP_SHARED, inFileD, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "Error: Failed to mmap the file \"%s\"!\n", inName);
        goto main_fail_2;
        return EXIT_FAILURE;
    }

#ifdef __USE_BSD
    /* Advise the OS that we plan to read the file sequentially: */
    (void) madvise(map, fileSize, MADV_SEQUENTIAL);
#endif

    /* Tokenize: */
    {
        size_t sl = 0u;
        size_t sc = 0u;
        ts = SMAS_tokenize((const char *) map, fileSize, &sl, &sc);
        if (unlikely(!ts)) {
            fprintf(stderr, "Error: Tokenization failed at (%zu,%zu)!\n", sl, sc);
            goto main_fail_3;
        }
        /* SMA_tokens_print(ts); */
    }

    /* Assemble the linking units: */
    {
        SMAS_LinkingUnits_init(&lus);
        const SMAS_Token * errorToken;
        char * errorString;
        SMAS_Assemble_Error r = SMAS_assemble(ts, &lus, &errorToken, &errorString);
        if (r != SMAS_ASSEMBLE_OK) {
            const char * smasErrorStr = SMAS_Assemble_Error_toString(r);
            assert(smasErrorStr);

            fprintf(stderr, "Error: ");

            if (errorToken)
                fprintf(stderr, "(%zu, %zu, %s) ", errorToken->start_line, errorToken->start_column, SMAS_TokenType_toString(errorToken->type) + 11);

            fprintf(stderr, "%s", smasErrorStr);
            if (errorString)
                fprintf(stderr, ": %s", errorString);
            fprintf(stderr, "\n");

            free(errorString);
            goto main_fail_4;
        }
        assert(!errorToken);
        assert(!errorString);
        SMAS_tokens_free(ts);

        if (munmap(map, fileSize) != 0)
            fprintf(stderr, "Error: Failed to munmap input file!\n");
        if (close(inFileD))
            fprintf(stderr, "Error: Failed to close input file!\n");
    } /* Active resources: lus */

    /* Generate the Sharemind Executable: */
    output = SMAS_link(0x0, &lus, &outputLength, 0);
    SMAS_LinkingUnits_destroy_with(&lus, &SMAS_LinkingUnit_destroy);
    if (!output) {
        fprintf(stderr, "Error generating output!\n");
        goto main_fail_1;
    }

    /* Open output file: */
    outFile = fopen(outName, "w");
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

    SMAS_LinkingUnits_destroy_with(&lus, &SMAS_LinkingUnit_destroy);
    SMAS_tokens_free(ts);

main_fail_3:

    if (munmap(map, fileSize) != 0)
        fprintf(stderr, "Error: Failed to munmap input file!\n");

main_fail_2:

    if (close(inFileD))
        fprintf(stderr, "Error: Failed to close input file!\n");

main_fail_1:

    return EXIT_FAILURE;
}
