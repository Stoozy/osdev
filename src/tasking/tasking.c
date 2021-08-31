#include "tasking.h"
#include "../memory/vmm.h"
#include "../memory/pmm.h"
#include "../string/string.h"
#include "../kprintf.h"
#include "../kmalloc.h"


TaskControlBlock * gp_task_queue;
TaskControlBlock * gp_current_task;

extern void load_pagedir(void * cr3);
extern void context_switch(Registers ** old, Registers * new);
extern void switch_to_task(Registers * new);

extern u8 stack[4096];
extern u64 g_ticks;

volatile u64 tasks;

static PageTable * get_current_cr3(){
    PageTable * current_cr3;
    asm volatile (" mov %%cr3, %0" : "=r"(current_cr3));
    return current_cr3;
}

static void dump_registers(Registers * regs){
    kprintf("[SCHEDULER]	RIP: %x\n", regs->rip);
    kprintf("[SCHEDULER]	RSP: %x\n", regs->rsp);
    kprintf("[SCHEDULER]	RDI: %x\n", regs->rdi);
    kprintf("[SCHEDULER]	RSI: %x\n", regs->rsi);
    kprintf("[SCHEDULER]	RBX: %x\n", regs->rbx);
    kprintf("[SCHEDULER]	RBP: %x\n", regs->rbp);

}

volatile void scheduler(Registers * regs){

    kprintf("[SCHEDULER]	Tasks: %llu\n", tasks);
    if(tasks == 0 || g_ticks % 30 != 0) return;


	kprintf("[SCHEDULER]	Switching tasks\n");
	kprintf("[SCHEDULER]	Old Task Registers (0x%x):  \n", &regs);
	dump_registers(regs);
	kprintf("[SCHEDULER]	New Task Registers (0x%x): \n", gp_current_task->context);
	dump_registers(&gp_current_task->context);

    // save registers
    memcpy(&gp_current_task->context, regs, sizeof(Registers));

	if(gp_current_task->next != NULL)
		gp_current_task = gp_current_task->next;
	else gp_current_task = gp_task_queue;

	switch_to_task(&gp_current_task->context);
    //load_pagedir(gp_current_task->cr3);
}

void idle_task(){
	for(;;) kprintf("Idling...\n");
}

void task_a(){
    for(;;) kprintf("Running task A\n");
}

volatile void create_task(void (*entrypoint)(void)){

    kprintf("[SCHEDULER]    Called create task with 0x%x\n", entrypoint);

    asm("cli");
    TaskControlBlock * p_tcb = kmalloc(sizeof(TaskControlBlock));

	memset((void*)p_tcb, 0x0, sizeof(TaskControlBlock));

    p_tcb->cr3 = get_current_cr3();
    p_tcb->next = NULL;

    kprintf("[SCHEDULER]    Created task with page dir at 0x%x\n", p_tcb->cr3);

	
    p_tcb->context.rip = (u64)entrypoint;
    p_tcb->context.rsp = (u64)pmm_alloc_block();


    /* append task */
    
    TaskControlBlock * current_task = gp_task_queue;

    if(current_task != NULL){
        while(current_task->next != NULL){
            current_task = current_task->next;
        }
        current_task->next = p_tcb;
    }else{
        // first task created
        gp_task_queue = p_tcb;
		gp_current_task = gp_task_queue;
    }

    ++tasks;

    asm("sti");
}

void multitasking_init(){

    tasks = 0;
    create_task(task_a);
    create_task(idle_task);


}




