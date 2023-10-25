#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    void *a = sf_malloc(8);
    void *b = sf_realloc(a, 10);
    sf_show_heap();
    void *c = sf_realloc(b, 4);
    sf_show_heap();

    double frag = sf_fragmentation();
    printf("\n%f\n", frag);

    sf_free(c);

    double util = sf_utilization();
    printf("\n%f\n", util);

    return EXIT_SUCCESS;
}