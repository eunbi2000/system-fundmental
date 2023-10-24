#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    void *x = sf_malloc((4096*3));
    sf_show_heap();
    sf_free(x);
    return EXIT_SUCCESS;
}