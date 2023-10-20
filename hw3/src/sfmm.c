/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"

#define PREV_BLOCK_ALLOCATED  0x4
#define THIS_BLOCK_ALLOCATED  0x8
#define BLOCK_MASK  0x00000000FFFFFFF0
#define PAYLOAD_MASK  0xFFFFFFFF00000000

sf_header set_header(size_t payload_size, size_t block_size, unsigned int alloc, unsigned int prev_alloc) {
    sf_header new = (sf_header)payload_size;
    new = new << 32;
    new = (new|block_size|(alloc*THIS_BLOCK_ALLOCATED)|(prev_alloc*PREV_BLOCK_ALLOCATED));
    return new;
}

int find_index(int size) {
    int min_size = 32;
    if (size < min_size) return -1;
    if (size == min_size) return 0; // M
    if (size <= (min_size)*2) return 1; // 2M
    if (size <= (min_size)*3) return 2; // 3M
    if (size <= (min_size)*5) return 3; // 3M --> 5M
    if (size <= (min_size)*8) return 4; // 5M --> 8M
    if (size <= (min_size)*13) return 5; // 8M --> 13M
    if (size <= (min_size)*21) return 6; // 13M --> 21M
    if (size <= (min_size)*34) return 7; // 21M --> 34M
    else return 8;
}

size_t check_size(size_t size) {
    size += 16;
    if (size % 32 == 0) return size; // if multiple of 32 or 32
    size_t result = size + (16 - (size % 16));
    return result;
}

void init_heap() {
    void* start = sf_mem_start();
    sf_block *prologue = (sf_block*)start; //start of prologue + 8byte for prev footer
    prologue->header = set_header(0,32,1,0); //32 is minimum block size
    prologue->body.links.prev = NULL;
    prologue->body.links.next = NULL;

    sf_block *wilder = (sf_block*)(start+32); //make free block all wilderness
    size_t total_size = (sf_mem_end()-16)-(start+32);
    wilder->prev_footer = prologue->header;
    wilder->header = set_header(0,total_size,0,1);

    sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next = wilder;
    sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.prev = wilder;
    wilder->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS - 1];
    wilder->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS - 1];

    sf_block *epilogue = (sf_block*)(sf_mem_end()-16); // consider prev_footer + header
    epilogue->prev_footer = wilder->header;
    epilogue->header = set_header(0,0,1,0);

    for (int i=0; i<NUM_FREE_LISTS-1; i++) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}

void *find_free(size_t payload_size, size_t block_size) {
    int index = find_index(block_size);
    if (index == -1) return NULL;
    for (int i=index; i<NUM_FREE_LISTS; i++) { //check until wilderness
        sf_block *block = sf_free_list_heads[i].body.links.next;
        void *block_add = block;
        while (block != &sf_free_list_heads[i]) {
            int curr_size = block->header & BLOCK_MASK;
            if (curr_size == block_size) {
                sf_block *newBlock = (sf_block *)block_add;
                newBlock->header = set_header(payload_size, block_size, 1, (block->header & THIS_BLOCK_ALLOCATED));

                sf_block *nextBlock = (sf_block *) (block_add + block_size);
                nextBlock->prev_footer = newBlock->header;
                nextBlock->header = nextBlock || PREV_BLOCK_ALLOCATED;

                //get rid from free block list
                sf_block *next = newBlock->body.links.next;
                sf_block *prev = newBlock->body.links.prev;
                next->body.links.prev = newBlock->body.links.prev;
                prev->body.links.next = newBlock->body.links.next;

                //since not free anymore, make null
                newBlock->body.links.next = NULL;
                newBlock->body.links.prev = NULL;

                return block;
            }
            else if (curr_size > block_size) { //if block size is greater than we want than we split
                // new allocated block
                sf_block *newBlock = (sf_block *) block_add;
                newBlock->header = set_header(payload_size, block_size, 1, (block->header & THIS_BLOCK_ALLOCATED));

                sf_block *next = newBlock->body.links.next;
                sf_block *prev = newBlock->body.links.prev;
                next->body.links.prev = newBlock->body.links.prev;
                prev->body.links.next = newBlock->body.links.next;

                // new free block
                size_t left_over = (size_t) (curr_size - block_size);
                sf_block *newFree = (sf_block *) (block_add + block_size);
                newFree->prev_footer = newBlock->header;
                newFree->header= set_header(0,left_over, 0, 1);

                //since not free anymore, make null
                newBlock->body.links.next = NULL;
                newBlock->body.links.prev = NULL;

                if (i == NUM_FREE_LISTS-1) { //if wilderness make the new free block as last
                    sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = newFree;
                    sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = newFree;
                    newFree->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
                    newFree->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
                }
                else {//if not, put it into the free list
                    int ind = find_index(left_over);
                    newFree->body.links.prev = sf_free_list_heads[ind].body.links.prev;
                    newFree->body.links.next = &sf_free_list_heads[ind];
                    (sf_free_list_heads[ind].body.links.prev)->body.links.next = newFree;
                    sf_free_list_heads[ind].body.links.prev = newFree;
                }
                // Free Block's Footer
                sf_block *new_next = (sf_block *) (block_add + curr_size);
                new_next->prev_footer = newFree->header;

                return block;
            }
            block = block->body.links.next;
        }
    }
    return NULL;
}

void *coalesce(void* block_add) {
    sf_block *block = block_add;
    size_t block_size = block->header & BLOCK_MASK;

    sf_block *nextBlock = (sf_block *)(block_add+block_size);
    int prev_alloc = block->prev_footer & THIS_BLOCK_ALLOCATED;
    int next_alloc = nextBlock->header & THIS_BLOCK_ALLOCATED;
    //four cases
    if (prev_alloc == 1 && next_alloc == 1) { //1. prev and next is allocated
        return block;
    }
    size_t prev_size = block->prev_footer & BLOCK_MASK;
    sf_block *prevBlock = (sf_block *)(block_add-prev_size);
    int prev_prev_alloc = prevBlock->prev_footer & THIS_BLOCK_ALLOCATED;
    if (prev_alloc == 0) { //2. if prev is free
        //remove from free list
        (prevBlock->body.links.prev)->body.links.next = prevBlock->body.links.next;
        (prevBlock->body.links.next)->body.links.prev = prevBlock->body.links.prev;
        prevBlock->body.links.prev = NULL;
        prevBlock->body.links.next = NULL;

        block_size = block_size + prev_size;
        block = (sf_block *)(block_add-prev_size); //make current block as prev
        block->header = set_header(0, block_size, 0, prev_prev_alloc);
        nextBlock->prev_footer = block->header;
    }
    size_t next_size = nextBlock->header & BLOCK_MASK;
    if (next_alloc == 0) { //3. if next is free
        (nextBlock->body.links.prev)->body.links.next = nextBlock->body.links.next;
        (nextBlock->body.links.next)->body.links.prev = nextBlock->body.links.prev;
        nextBlock->body.links.prev = NULL;
        nextBlock->body.links.next = NULL;

        block_size = block_size + next_size;
        block->header = set_header(0, block_size, 0, (block->header & PREV_BLOCK_ALLOCATED));
        nextBlock = (sf_block *)(block_add + block_size + next_size);
        nextBlock->prev_footer = block->header;
    }
    return block;
}

int put_free(sf_block *block) {
    //set header
    block->header = set_header(0, (block->header & BLOCK_MASK), 0, (block->header & PREV_BLOCK_ALLOCATED));
    //set prev footer and next block's header
    sf_block *nextBlock = (sf_block *) (block+(block->header & BLOCK_MASK));
    nextBlock->prev_footer = block->header;
    nextBlock->header = nextBlock->header | (0*PREV_BLOCK_ALLOCATED);
    //set next block's footer
    sf_block *next_nextBlock = (sf_block *) (nextBlock+(nextBlock->header & BLOCK_MASK));
    next_nextBlock->prev_footer = nextBlock->header;
    //coalesce if needed
    sf_block *free_block = (sf_block *)(coalesce(block));

    size_t free_size = free_block->header & BLOCK_MASK;
    int index = find_index(free_size);
    if (index == -1) {
        return -1;
    }
    if (next_nextBlock == (sf_mem_end()-16)) { //means wilder block
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = free_block;
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = free_block;
        free_block->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
        free_block->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
    }
    else { //put into free list
        free_block->body.links.prev = sf_free_list_heads[index].body.links.prev;
        free_block->body.links.next = &sf_free_list_heads[index];
        (sf_free_list_heads[index].body.links.prev)->body.links.next = free_block;
        sf_free_list_heads[index].body.links.prev = free_block;
    }
    return 0;
}

int create_new_page() {
    void *prev_end = sf_mem_grow();
    if (prev_end == NULL) {
        return -1;
    }
    //get old epilogue
    sf_header old_epi = (sf_header) (prev_end-16);
    int prev_alloc = old_epi & PREV_BLOCK_ALLOCATED;
    size_t prev_size = old_epi & BLOCK_MASK;
    //set new epi header
    sf_block *new_epi = (sf_block *) (sf_mem_end()-16);
    new_epi->header = set_header(0,0,1,0);
    sf_block *wilder;

    if (prev_alloc == 1) { //wilderness used up
        wilder = (sf_block *) (prev_end-16);
        wilder->header = set_header(0, (PAGE_SZ-16), 0, prev_alloc);
        new_epi->prev_footer = wilder->header;
    }
    else { //wilder still has mem
        wilder = (sf_block *)((prev_end-16)-prev_size); //get prev wilder header and change size
        wilder->header = set_header(0, (prev_size+PAGE_SZ-16), 0, (wilder->header & PREV_BLOCK_ALLOCATED));
        new_epi->prev_footer = wilder->header;
    }

    if (put_free(wilder) == -1) {
        return -1;
    }
    return 0;
}

void *sf_malloc(size_t size) { //still have to do growing page
    sf_block *allocated = NULL;
    if (size <= 0) {
        return NULL;
    }
    if (sf_mem_start() == sf_mem_end()) {
        void *grow = sf_mem_grow();
        if (grow != NULL) {
            init_heap();
        }
    }
    while (allocated == NULL) { //while it is NULL grow if needed
        size_t block_size = check_size(size);
        allocated = (sf_block*) find_free(size, block_size);
        // sf_show_heap();
        if (allocated == NULL) { // call page grow
            if (create_new_page() == -1) {
                sf_errno = ENOMEM; //set error to ENOMEM
                return NULL;
            }
        }
    }
    return allocated->body.payload;
}

int check_pointer(void *pp) {
    sf_block *free_block = (sf_block *) (pp); //since initial address is to payload
    size_t block_size = free_block->header & BLOCK_MASK;
    size_t payload_size = (free_block->header & PAYLOAD_MASK) >> 32;
    if ((void *)free_block >= sf_mem_end() || (void *)free_block <= sf_mem_start()) { //if ptr is not valid
        return -1;
    }
    if (block_size < 32 || (block_size % 16) !=0) { //if block size is less than 32 or not multiples of 16
        return -1;
    }
    if (payload_size <= 0 || payload_size >= block_size) { //if payload size is less than 0 or greater than block size
        return -1;
    }
    if ((free_block->header & THIS_BLOCK_ALLOCATED) == 0) { //if this block isn't allocated
        return -1;
    }
    return 0;
}

void sf_free(void *pp) {
    if (pp == NULL) { //if pointer is not multiple of 16
        abort();
    }
    pp = (pp-16); //since initial address is to payload
    if (check_pointer(pp) == -1) {
        abort();
    }
    sf_block *valid = (sf_block *) pp;
    if (put_free(valid) != 0) {
        abort();
    }
}

void *sf_realloc(void *pp, size_t rsize) {
    // To be implemented.
    abort();
}

double sf_fragmentation() {
    return 0.0;
}

double sf_utilization() {
    // if (sf_mem_start() != sf_mem_end()) {

    // }
    return 0.0;
}
