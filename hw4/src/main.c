#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "deet.h"
#include "helper.h"

int main(int argc, char *argv[]) {
    // TO BE IMPLEMENTED
    // Remember: Do not put any functions other than main() in this file.
    int i=0;
    if (argc == 2) {
        if (strcmp(*(argv+1), "-p") == 0) {
            i=1;
        }
    }
    if (init(i) == 0)
        return EXIT_SUCCESS;
    return EXIT_FAILURE;
}
