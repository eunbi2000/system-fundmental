#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    void *x = sf_malloc(sizeof(double) * 8);
    void *y = sf_realloc(x, sizeof(int));

    printf("\n !!! DONE MALLOC !!!\n ");
    sf_free(y);

    return EXIT_SUCCESS;
}