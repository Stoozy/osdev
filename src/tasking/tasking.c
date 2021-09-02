#include "tasking.h"
#include "../memory/pmm.h"
#include "../kprintf.h"
#include "../kmalloc.h"
#include "../string/string.h"

#define MAX_PROCS   256

ProcessControlBlock * gp_process_queue;
ProcessControlBlock * gp_current_process;

ProcessControlBlock p_table[MAX_PROCS];

extern void switch_to_process(ProcessControlBlock ** old, ProcessControlBlock * new);
extern u64 g_ticks;

volatile u64 g_procs;

void task_a(void);
void idle_task(void);

void idle_task(){ 
    for(;;) {
        kprintf("Idling...[%d processes]\n", g_procs); 
    }
}

void task_a(void){ 
    for(;;) kprintf("Running task A...\n");
}

void dump_regs(void * stack){
    Registers * regs = (Registers*) stack;
    kprintf("[SCHEDULER]    RIP: 0x%x\n", regs->rip);
    kprintf("[SCHEDULER]    RSP: 0x%x\n", regs->rsp);
    kprintf("[SCHEDULER]    RBP: 0x%x\n", regs->rbp);
    kprintf("[SCHEDULER]    RBX: 0x%x\n", regs->rbx);
    kprintf("[SCHEDULER]    RSI: 0x%x\n", regs->rsi);
    kprintf("[SCHEDULER]    RDI: 0x%x\n", regs->rdi);
}

void schedule(){
    kprintf("[SCHEDULER]    %d Global Processes\n", g_procs);
    if(gp_process_queue == NULL || g_ticks % 20 != 0) return;

    ProcessControlBlock * current_pcb = gp_process_queue;

    // second task doesn't exist, 
    // no need to switch
    // just return
    if(gp_process_queue->next == NULL) return;

    // reached tail, go to head
    if(gp_current_process->next == NULL){
        dump_regs(gp_process_queue->stack_top);
        switch_to_process(&gp_current_process, gp_process_queue);
        gp_current_process = gp_process_queue;
        return;
    }

    // just go to next process 
    dump_regs(gp_process_queue->next->stack_top);
    switch_to_process(&gp_current_process, gp_current_process->next->stack_top);
    gp_current_process = gp_current_process->next;

}

ProcessControlBlock * create_process(void (*entry)(void)){
    ProcessControlBlock * pcb = pmm_alloc_block();

    pcb->stack_top = (void*)((u64)pmm_alloc_block()+0x1000);

    Registers * regs = (Registers*) pcb->stack_top;
    memset(regs, 0, sizeof(Registers));
    regs->rip = (u64)entry;
    regs->rsp = (u64)pcb->stack_top;

    pcb->cr3 = vmm_get_current_cr3();
    pcb->next = NULL;

    return pcb; 
}

void register_process(ProcessControlBlock * new_pcb){
    ProcessControlBlock * current_pcb = gp_process_queue;  

    // first process
    if(current_pcb == NULL){
        gp_process_queue = new_pcb;
        gp_current_process = gp_process_queue;
    }

    while(current_pcb->next != NULL)
        current_pcb = current_pcb->next;
    current_pcb->next = new_pcb;

    ++g_procs;
}

void multitasking_init(){
    gp_process_queue = NULL;
    gp_current_process = NULL;

    asm volatile("cli");
    g_procs = 0;

    gp_process_queue = create_process(idle_task);
    ++g_procs;

    gp_current_process = gp_process_queue;
    gp_current_process->next = create_process(task_a);
    ++g_procs;

    //ProcessControlBlock * new_pcb = create_process(idle_task);
    //register_process(new_pcb);

    //new_pcb = create_process(task_a);
    //register_process(new_pcb);

    asm volatile("sti");
}
