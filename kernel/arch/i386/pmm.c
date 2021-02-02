#include <kernel/pmm.h>
#include <kernel/util.h>

#include <string.h>
#include <stddef.h>

#define BLOCK_SIZE          4096
#define BLOCKS_PER_BYTE     8
#define MAX_BITMAPS         131072

static uint32_t total_blocks;
static uint32_t total_bmaps;
static uint32_t free_blocks=0;
static uint32_t used_blocks=MAX_BITMAPS * BLOCK_SIZE;
static uint32_t last_checked_block = 1;

static char mmap[MAX_BITMAPS]; // max maps for a 4 GiB memory space

static void set_frame_used(uint32_t n_block){
    return _set_bit(mmap[n_block], n_block%8);
}
static void set_frame_free(uint32_t n_block){
    return _clear_bit(mmap[n_block], n_block%8);
}


void pmm_init(){
    total_blocks = MAX_BITMAPS*BLOCKS_PER_BYTE;
    total_bmaps = total_blocks/BLOCKS_PER_BYTE;
    
    // every frame is used by default
    memset(&mmap, 0xff, total_bmaps);
}


void pmm_init_region(void * addr, size_t size){
    uint32_t start_frame = (uint32_t)addr/BLOCK_SIZE;
    uint32_t end_frame = start_frame + size/BLOCK_SIZE;

    for(uint32_t i=start_frame; i<end_frame; i++){
       set_frame_free(i);
       free_blocks++;
       used_blocks--;
    }

    return; 
}

static int pmm_get_first_free(){
    for(; last_checked_block<total_blocks; last_checked_block++){
        if(!(_check_bit(mmap[last_checked_block/8],last_checked_block%8))){
            return last_checked_block;
        }
    }
    last_checked_block = 0;
    return -1;
}

void * pmm_alloc_block(){
    int block = pmm_get_first_free(); 

    if(block == -1){
        /*
         * do another search beginning at frame 0 
         * to make sure no frames are left free
         */

        block = pmm_get_first_free();
        if(block == -1) return 0x0; // ran out of usable mem
    } 
    uint32_t addr = block * BLOCK_SIZE;

    // set block used
    set_frame_used(block);
    used_blocks++;
    free_blocks--;

    return  (void*) addr;
}

void pmm_free_block(uint32_t addr){
    int block =  addr/BLOCK_SIZE;
    set_frame_free(block);

    used_blocks--;
    free_blocks++;
    return;
}


uint32_t pmm_get_free_block_count(){
    return free_blocks;
}

uint32_t pmm_get_block_count(){
    return total_blocks;
}


