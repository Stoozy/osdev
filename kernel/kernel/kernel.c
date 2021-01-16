/*Copyright 2021 Stoozy 

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <kernel/tty.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/io.h>
#include <kernel/rtc.h>
#include <kernel/util.h>
#include <kernel/ata.h>
#include <kernel/pmm.h>
#include <kernel/paging.h>

//#include <kernel/vmm.h>

#include "multiboot.h"

#define INT_MAX 2147483647

#define PAGE_PRESENT    0
#define PAGE_RW         1


void Sleep(uint32_t ms);
void ATA_WAIT_INT();
uint32_t placement_address = 0;

extern void load_page_directory(uint32_t *);
extern void init_paging();
extern uint32_t ekernel;
extern uint32_t sbss;

typedef struct multiboot_mmap_entry mmap_entry_t;

static inline bool are_ints_enabled(){
    uint64_t flags;
    asm volatile("pushf\n\t" "pop %0" : "=g"(flags));
    return flags & (1 << 9);
}

int oct2bin(unsigned char *str, int size) {
    int n = 0;
    unsigned char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

/* returns file size and pointer to file data in out */
int tar_lookup(unsigned char *archive, char *filename, char **out) {
    unsigned char *ptr = archive;
 
    while (!memcmp(ptr + 257, "ustar", 5)) {
        int filesize = oct2bin(ptr + 0x7c, 11);
        if (!memcmp(ptr, filename, strlen(filename) + 1)) {
            *out = ptr + 512;
            return filesize;
        }
        ptr += (((filesize + 511) / 512) + 1) * 512;
    }
    return 0;
}

void idpaging(uint32_t *first_pte, uint32_t from, int size) {
	from = from & 0xfffff000; // discard bits we don't want
	for(; size>0; from += 4096, size -= 4096, first_pte++){
	   *first_pte = from | 1;     // mark page present.
	}
}

void kernel_main(multiboot_info_t* mbd, unsigned int magic) {
	terminal_initialize();
    printf("Initialized terminal\n");
    init_gdt();

    init_idt();

	printf("Kernel main loaded at: 0x%x\n", kernel_main);

	printf("magic is: %x\n", magic);

    
    bool interrupt_status  = are_ints_enabled();
    if(interrupt_status) printf("Interrupt requests are currently enabled\n");
    else printf("Interrupt requests are currently disabled\n");

	printf("MMAP_ADDR: 0x%x \nMEM_LOW:%d\nMEM_UPPER:%d\n", mbd->mmap_addr, mbd->mem_lower, mbd->mem_upper);

    uint32_t avail_mmap[3][2]= {0}; // first col: addr, second col: size
    uint64_t ll = 32768 *1024;

	int entries = 0, avail_entries=0;
	mmap_entry_t* entry = mbd->mmap_addr;
    uint64_t total_mem_size_kib =0;
    uint32_t mem_end_page=0;	
	while(entry < (mbd->mmap_addr + mbd->mmap_length)) {
		entry = (mmap_entry_t*) ((unsigned int) entry + entry->size + sizeof(entry->size));
        if(entry->type == MULTIBOOT_MEMORY_AVAILABLE){
            printf("Entry #%d:\n", entries)	;
            printf("    Size: %d\n", entry->size); 
            printf("    Address: 0x%x\n", entry->addr);
            printf("    Length: %lluMiB\n", (entry->len/(1024*1024)));
            printf("    Type: %d\n", entry->type);
            total_mem_size_kib += (entry->len)/1024;
            // fill available entry
            avail_mmap[avail_entries][0] = entry->addr;
            avail_mmap[avail_entries][1] = entry->len;
            avail_entries++;
        }
		entries++;
	}

    printf("Total available memory: %d MiB\n", (total_mem_size_kib/(1024))); // converto MiB

    //printf("End of kernel is at: 0x%x\n",ekernel );
    
    pmm_init(total_mem_size_kib);

    printf("Initialized Physical Memory Manager with %ldKiB (%d blocks)\n", total_mem_size_kib, pmm_get_block_count());


	uint32_t i=0;
    while(avail_mmap[i][1] != 0){
        //printf("Addr: 0x%x Size: %ld KiB\n", avail_mmap[i][0], avail_mmap[i][1]);
        pmm_init_region(avail_mmap[i][0], avail_mmap[i][1]);
        i++;
    }
    printf("%d free physical blocks\n", pmm_get_free_block_count()) ;
    uint32_t directories[1024] __attribute__((aligned(4096))); 
    uint32_t first_tab[1024] __attribute__((aligned(4096)));

    // page map the first 4MiB
    for(i=0; i<1024; i++){
        first_tab[i] = (i*4096) | 3;
    }

    // fill the rest of the memory (4MiB-4GiB)
    directories[0]= ((uint32_t) first_tab)|3;
    for(i=1; i<1023; i++){ // iterates directories
        uint32_t offset=i*4096;
        uint32_t table[1024] __attribute__((aligned(4096)));
        for(int j=0; j<1024; j++){
            table[j] = (offset + (j*4096))|3;
        }
        directories[i]= ((uint32_t)table)|3;
    }
    
	load_page_directory((uint32_t *) &directories);
	init_paging();
    printf("Paging is now enabled\n");
     
  
    idpaging(&first_tab, 0, 1024*4096); // 4MiB
    printf("Identity mapped first 4 MiB\n");


	read_rtc();
    terminal_setcolor(0xE); // yellow
    printf("%s"," _ _ _     _                      _              _____ _____ \n");
    printf("%s","| | | |___| |___ ___ _____ ___   | |_ ___    ___|     |   __|\n");
    printf("%s","| | | | -_| |  _| . |     | -_|  |  _| . |  |- _|  |  |__   |\n");
    printf("%s","|_____|___|_|___|___|_|_|_|___|  |_| |___|  |___|_____|_____|\n");
    terminal_setcolor(0xF); // white

	
    for(;;){
		asm("hlt");
    }
}
