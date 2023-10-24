#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    void *x = sf_malloc(480);
    void *y = sf_realloc(x, sizeof(int));

    sf_free(y);
    return EXIT_SUCCESS;
}