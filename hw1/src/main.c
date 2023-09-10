#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "debug.h"

int main(int argc, char **argv)
{
    if(validargs(argc, argv))
        USAGE(*argv, EXIT_FAILURE);
    if(global_options == HELP_OPTION)
        USAGE(*argv, EXIT_SUCCESS);
    if(global_options == 0) {
        int success_read = read_distance_data(stdin);
        if (success_read)
            return EXIT_SUCCESS;
    }

    // TO BE IMPLEMENTED
    return EXIT_FAILURE; // if successful with everything
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
