#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "debug.h"

int main(int argc, char **argv)
{
    if(validargs(argc, argv)) {
        USAGE(*argv, EXIT_FAILURE);
    }
    if(global_options == HELP_OPTION) {
        USAGE(*argv, EXIT_SUCCESS);
    }
    int success_read = read_distance_data(stdin);

    if(global_options == 0) {
        int success_print = build_taxonomy(stdout);
        if (success_read ==0 && success_print==0)
            return EXIT_SUCCESS;
        else {
            return EXIT_FAILURE;
        }
    }
    if(global_options == NEWICK_OPTION) {
        printf("\n goes in newick");
        int success_newick = emit_newick_format(stdout);
        if (success_newick==0 && success_read==0) {
            return EXIT_SUCCESS;
        }
        else {
            return EXIT_FAILURE;
        }
    }
    if(global_options == MATRIX_OPTION) {
        int success_distance_matrix = emit_distance_matrix(stdout);
        if (success_distance_matrix==0 && success_read==0) {
            return EXIT_SUCCESS;
        }
        else {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS; // if successful with everything
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
