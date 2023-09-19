#include <stdlib.h>

#include "global.h"
#include "debug.h"

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the various options that were specified will be
 * encoded in the global variable 'global_options', where it will be
 * accessible elsewhere in the program.  For details of the required
 * encoding, see the assignment handout.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * @modifies global variable "global_options" to contain an encoded representation
 * of the selected program options.
 */
int validargs(int argc, char **argv)
{
    if (argc < 1) {
        return -1;
    }
    argv++; //checks first input
    char* charptr = *argv; // points to the first char of input

    if (argc > 1) {
        if (*charptr == '-') {
            charptr++;
            if (*charptr == 'h') {
                global_options |= HELP_OPTION;
                return 0;
            }
            else if (*charptr == 'm' && *(charptr +1) == 0) {
                if (argc == 2) {
                    global_options |= MATRIX_OPTION;
                    return 0;
                }
                else {
                    return -1;
                }
            }
            else if (*charptr == 'n' && *(charptr +1) == 0) {
                if (argc > 2) {
                    argv++;
                    charptr = *argv;
                    if (*charptr == '-') {
                        charptr++;
                        if (*charptr == 'o' && argc == 4 && *(charptr +1) == 0) {
                            argv++;
                            charptr = *argv;
                            global_options |= NEWICK_OPTION;
                            outlier_name = charptr;
                        }
                        else {
                            return -1;
                        }
                    }
                }
                else if (argc == 2){
                    global_options |= NEWICK_OPTION;
                    outlier_name = '\0';
                    return 0;
                }
                else {
                    return -1;
                }
            }
        }
        else {
            return -1;
        }
    }
    return 0;
}
