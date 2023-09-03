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
    // TO BE IMPLEMENTED
    return EXIT_FAILURE; 
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
