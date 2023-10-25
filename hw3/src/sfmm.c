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
#define max(x,y) (((x) >= (y)) ? (x) : (y))

double total_payload = 0.0;
double total_allocated = 0.0;
double max_aggregate = 0.0;

sf_header set_header(size_t payload_size, size_t block_size, unsigned int alloc, unsigned int prev_alloc) {
    if (alloc != 0) {
        alloc = 1;
    }
    if (prev_alloc != 0) {
        prev_alloc = 1;
    }
    sf_header new = (sf_header)payload_size;
    new = new << 32;
    new = (new|block_size|(alloc*THIS_BLOCK_ALLOCATED)|(prev_alloc*PREV_BLOCK_ALLOCATED));
    return new;
}

int find_index(int size) {
    int fib_a=1;
    int fib_b=1;
    int fib = 1;
    int min_size = 32;
    if (size < min_size) {
        return -1;
    }
    for(int i=0; i<NUM_FREE_LISTS-2; i++)
    {
        if(size <= fib*min_size) return i;
        fib_a = fib_b;
        fib_b = fib;
        fib = fib_b + fib_a;
    }
    return NUM_FREE_LISTS-2;
}

size_t check_size(size_t size) {
    size += 16;
    if (size >= 32 && size % 16 == 0) return size; // if multiple of 32 or 32
    size_t result = size + (16 - (size % 16));
    return result;
}

void init_heap() {
    void* start = sf_mem_start();
    sf_block *prologue = (sf_block*)start; //start of prologue + 8byte for prev footer
    prologue->header = set_header(0,32,1,0); //32 is minimum block size

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
    sf_block *block;
    int index = find_index(block_size);
    for (int i=index; i<NUM_FREE_LISTS; i++) { //check until wilderness
        block = &sf_free_list_heads[i];
        void *block_add = block;
        while (block->body.links.next != &sf_free_list_heads[i]) {
            block = block->body.links.next;
            block_add = block;
            int curr_size = block->header & BLOCK_MASK;
            if (curr_size == block_size) { //have to account for if the wilderness is used up
                sf_block *newBlock = (sf_block *)block_add;
                newBlock->header = set_header(payload_size, block_size, 1, (block->prev_footer & THIS_BLOCK_ALLOCATED));

                sf_block *nextBlock = (sf_block *) (block_add + block_size);
                nextBlock->prev_footer = newBlock->header;
                size_t nextBlock_payload = (nextBlock->header & PAYLOAD_MASK) >> 32;
                size_t nextBlock_size = nextBlock->header & BLOCK_MASK;
                int nextBlock_alloc = nextBlock->header & THIS_BLOCK_ALLOCATED;
                nextBlock->header = set_header(nextBlock_payload, nextBlock_size, nextBlock_alloc, 1);

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
                newBlock->header = set_header(payload_size, block_size, 1, (block->header & PREV_BLOCK_ALLOCATED));

                sf_block *next = newBlock->body.links.next;
                sf_block *prev = newBlock->body.links.prev;
                next->body.links.prev = newBlock->body.links.prev;
                prev->body.links.next = newBlock->body.links.next;

                // new free block
                size_t left_over = (size_t) (curr_size - block_size);
                if (left_over >= 32) { //no splinters
                    sf_block *newFree = (sf_block *) (block_add + block_size);
                    newFree->prev_footer = newBlock->header;
                    newFree->header= set_header(0,left_over, 0, 1);
                    if (i == NUM_FREE_LISTS-1) { //if the block is wilderness make the new free block as last
                        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = newFree;
                        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = newFree;
                        newFree->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
                        newFree->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
                    }
                    else {//if not, put it into the free list
                        int ind = find_index(left_over);
                        newFree->body.links.next = sf_free_list_heads[ind].body.links.next;
                        newFree->body.links.prev = &sf_free_list_heads[ind];
                        (sf_free_list_heads[ind].body.links.next)->body.links.prev = newFree;
                        sf_free_list_heads[ind].body.links.next = newFree;
                    }
                    // Free Block's Footer
                    sf_block *new_next = (sf_block *) (block_add + curr_size);
                    new_next->prev_footer = newFree->header;
                }
                else { // put whole size without spliting into size
                    newBlock->header = set_header(payload_size, curr_size, 1, (block->header & PREV_BLOCK_ALLOCATED));
                    sf_block *nextBlock = (sf_block *)(block_add+curr_size);
                    nextBlock->prev_footer = newBlock->header;
                }

                //since not free anymore, make null
                newBlock->body.links.next = NULL;
                newBlock->body.links.prev = NULL;

                return block;
            }
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
    if (prev_alloc != 0 && next_alloc != 0) { //1. prev and next is allocated
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
        nextBlock = (sf_block *)((void *)block + block_size);
        nextBlock->prev_footer = block->header;
    }
    return block;
}

int put_free(void *block_add) {
    int wilder = 0;
    sf_block *block = (sf_block *) block_add;
    //set header
    size_t block_size = (block->header & BLOCK_MASK);
    block->header = set_header(0, block_size, 0, (block->header & PREV_BLOCK_ALLOCATED));

    //set prev footer and next block's header & check if wilder first
    sf_block *nextBlock = (sf_block *) (block_add+block_size);
    nextBlock->prev_footer = block->header;
    if ((void *)nextBlock != sf_mem_end()-16) { //if next block is not epi
        size_t nextBlock_size = (nextBlock->header & BLOCK_MASK);
        size_t nextBlock_payload = (nextBlock->header & PAYLOAD_MASK) >> 32;
        nextBlock->header = set_header(nextBlock_payload, nextBlock_size,
         (nextBlock->header & THIS_BLOCK_ALLOCATED), 0);

        //set next block's footer
        sf_block *next_nextBlock = (sf_block *) (block_add+block_size+nextBlock_size);
        next_nextBlock->prev_footer = nextBlock->header;
    }
    else {
        wilder = 1;
    }
    //coalesce
    sf_block *free_block = (sf_block *)(coalesce(block));

    size_t free_size = free_block->header & BLOCK_MASK;

    sf_block *after_free = (sf_block *)((void*)free_block + free_size);
    if ((void *)after_free == sf_mem_end()-16) {
    //if after coalesce, block becomes wilder meaning next next block was epii
        wilder = 1;
    }

    int index = find_index(free_size);
    if (wilder == 1) { //means wilder block
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = free_block;
        sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = free_block;
        free_block->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];
        free_block->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
    }
    else { //put into free list
        free_block->body.links.next = sf_free_list_heads[index].body.links.next;
        free_block->body.links.prev = &sf_free_list_heads[index];
        (sf_free_list_heads[index].body.links.next)->body.links.prev = free_block;
        sf_free_list_heads[index].body.links.next = free_block;
    }
    return 0;
}

int create_new_page() {
    void *prev_end = sf_mem_grow();
    if (prev_end == NULL) {
        return -1;
    }
    //get old epilogue
    sf_block *old_epi = (sf_block *) (prev_end-16);
    int prev_alloc = (old_epi->header & PREV_BLOCK_ALLOCATED);
    size_t prev_size = old_epi->prev_footer & BLOCK_MASK;

    //set new epi header
    sf_block *new_epi = (sf_block *) (sf_mem_end()-16);
    new_epi->header = set_header(0,0,1,0);
    sf_block *wilder;

    if (prev_alloc != 0) { //wilderness used up
        wilder = (sf_block *) (prev_end-16);
        wilder->header = set_header(0, (PAGE_SZ), 0, (old_epi->header & PREV_BLOCK_ALLOCATED));
        new_epi->prev_footer = wilder->header;
    }
    else { //wilder still has mem
        wilder = (sf_block *)((prev_end-16)-prev_size); //get prev wilder header and change size
        wilder->header = set_header(0, (prev_size+PAGE_SZ), 0, (old_epi->prev_footer & PREV_BLOCK_ALLOCATED));
        new_epi->prev_footer = wilder->header;
    }
    put_free(wilder);
    return 0;
}

void *sf_malloc(size_t size) {
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
    size_t block_size = check_size(size);
    while (allocated == NULL) { //while it is NULL grow if needed
        allocated = (sf_block*) find_free(size, block_size);
        if (allocated == NULL) { // call page grow
            if (create_new_page() == -1) {
                sf_errno = ENOMEM; //set error to ENOMEM
                return NULL;
            }
        }
    }
    total_payload += size;
    total_allocated += block_size;
    max_aggregate = max(total_payload, max_aggregate);
    return allocated->body.payload;
}

int check_pointer(void *pp) {
    sf_block *free_block = (sf_block *) (pp); //since initial address is to payload
    size_t block_size = free_block->header & BLOCK_MASK;
    if ((size_t)pp % 16 != 0) { //if address is not multiple of 16
        return -1;
    }
    if ((void *)free_block+block_size > sf_mem_end()-8 || (void *)free_block < sf_mem_start()+32) { //if ptr is not valid
        return -1;
    }
    if (block_size < 32 || (block_size % 16) !=0) { //if block size is less than 32 or not multiples of 16
        return -1;
    }
    if ((free_block->header & THIS_BLOCK_ALLOCATED) == 0 ||
        ((free_block->header & PREV_BLOCK_ALLOCATED) == 0 && (free_block->prev_footer & THIS_BLOCK_ALLOCATED) != 0)) { 
        //if this block isn't allocated or prev allocated is 0 but footer says it's 1
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
    size_t payload_size = (valid->header & PAYLOAD_MASK) >> 32;
    size_t block_size = (valid->header & BLOCK_MASK);
    if (put_free(valid) != 0) {
        abort();
    }
    total_payload -= payload_size;
    total_allocated -= block_size;
    max_aggregate = max(total_payload, max_aggregate);
}

void *sf_realloc(void *pp, size_t rsize) {
    if (pp == NULL) {
        sf_errno = EINVAL;
        return NULL;
    }
    if (check_pointer(pp-16) == -1) { //since initial address is to payload
        sf_errno = EINVAL;
        return NULL;
    }
    if (rsize == 0) {
        sf_free(pp);
        return NULL;
    }
    sf_block * original = (sf_block *) (pp-16);
    size_t original_payload = (original->header & PAYLOAD_MASK) >>32;

    if (original_payload == rsize) { //if reallocing to same size
        return pp;
    }
    size_t original_block_size = check_size(original_payload);
    size_t realloc_block_size = check_size(rsize);
    size_t left_over = original_block_size - realloc_block_size;
    int prev_alloc = original->header & PREV_BLOCK_ALLOCATED;

    if (original_payload < rsize) { //if we realloc to larger block
        if (original_block_size == realloc_block_size) { //if block size still remains same
            original->header = set_header(rsize, original_block_size, 1, prev_alloc);
            sf_block * nextBlock = (sf_block *) (pp -16 + original_block_size);
            nextBlock->prev_footer = original->header;
            total_payload = total_payload + rsize - original_payload;
            max_aggregate = max(total_payload, max_aggregate);
            return original->body.payload;
        }
        else {
            void *realloc = sf_malloc(rsize); //malloc returns payload
            if (realloc == NULL) {
                sf_errno = ENOMEM;
                return NULL;
            }
            memcpy(realloc, pp, original_payload);
            sf_free(pp);
            return realloc;
        }
    }
    else if (original_payload > rsize) { //if we realloc to smaller block
        //check if causes splinters
        if (left_over < 32) { //causes splinter so keep the block size as before
            original->header = set_header(rsize, original_block_size, 1, prev_alloc);
            sf_block * nextBlock = (sf_block *) (pp -16 + original_block_size);
            nextBlock->prev_footer = original->header;
        }
        else { //assign new size and put new free block into free list
            original->header = set_header(rsize, realloc_block_size, 1, prev_alloc);
            sf_block * free_block = (sf_block *) (pp -16 + realloc_block_size);
            free_block->prev_footer = original->header;
            free_block->header = set_header(0, (original_block_size-realloc_block_size), 0, 1);
            sf_block * nextBlock = (sf_block *) (pp -16 + original_block_size);
            nextBlock->prev_footer= free_block->header;

            total_allocated = total_allocated + realloc_block_size - original_block_size;
            put_free(free_block);
        }
        total_payload = total_payload + rsize - original_payload;
        max_aggregate = max(total_payload, max_aggregate);
        return original->body.payload;
    }
    return NULL;
}

double sf_fragmentation() { // total payload / total allocated blocks
    double frag = 0.0;
    if (total_allocated == 0) {
        return frag;
    }
    frag = total_payload/total_allocated;
    return frag;
}

double sf_utilization() { // max aggregate payload / heap size
    double util = 0.0;
    if (sf_mem_start() == sf_mem_end()) {
        return util;
    }
    double heap = (double)(sf_mem_end() - sf_mem_start());
    util = max_aggregate / heap;
    return util;
}
