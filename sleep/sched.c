#include "embryos.h"

#define N_PRIORITIES 3

struct pcb *run_queue[N_PRIORITIES];
struct pcb *sleep_queue[N_PRIORITIES];

void sched_resume(struct pcb *pcb) { proc_enqueue(&run_queue[0], pcb); }

void sched_check_sleeping_procs(void) {
    uint64_t curr_ticks = mtime_get();
    uint64_t curr_ns = curr_ticks * 1000000000 / time_base;
    for (int p = 0; p < N_PRIORITIES; p++) {
        struct pcb *fst_repeat = nullptr;
        while (sleep_queue[p] && sleep_queue[p] != fst_repeat) {
            struct pcb *pcb = proc_dequeue(&sleep_queue[p]);
            if (curr_ns >= pcb->deadline) {
                kprintf("Proc %p done sleeping at %u\n", pcb, curr_ns);
                pcb->status = PROC_WAITING;
                pcb->deadline = 0;
                proc_enqueue(&run_queue[p], pcb);
            } else { // enqueue back if needs more sleep
                proc_enqueue(&sleep_queue[p], pcb);
                if (fst_repeat == nullptr) fst_repeat = pcb;
            }
        }
    }
}

void sched_block(struct pcb *old) {
    int p = 0;      // first find highest priority process

    sched_check_sleeping_procs(); // wake up sleeping procs if necessary

    while (p < N_PRIORITIES && run_queue[p] == 0) p++;
    struct pcb *new = proc_dequeue(&run_queue[p]);

    if (new != old) {       // switch needed
        if (old->status == PROC_RUNNING) old->status = PROC_WAITING;
        new->hart = old->hart;
        new->hart->needs_tlb_flush = 1;
        new->status = PROC_RUNNING;
        new->deadline = 0;
        sched_set_self(new);
        L3(L_FREQ, L_CTX_SWITCH, (uintptr_t) old, (uintptr_t) new, new->hart->id);
        ctx_switch(&old->sp, new->sp);
        reap_zombies();
    }
}

void sched_run(int executable, struct rect area, void *args, int size) {
    struct pcb *old = sched_self();
    proc_enqueue(&run_queue[0], old);
    struct pcb *new = proc_create(old->hart, executable, area, args, size);
    new->status = PROC_RUNNING;
    sched_set_self(new);
    L4(L_NORM, L_CTX_START, (uintptr_t) old, (uintptr_t) new, new->hart->id, executable);
    ctx_start(&old->sp, (char *) new + PAGE_SIZE);
    reap_zombies();
}

void sched_yield(void) {
    struct pcb *current = sched_self();
    if (current->executable > 0) {     // idle loop doesn't yield at interrupts
        if (current->status == PROC_RUNNING)
            proc_enqueue(&run_queue[1], current);
        else if (current->status == PROC_SLEEPING) 
            proc_enqueue(&sleep_queue[1], current);
        else {
            // kprintf("[WARNING] Unhandled process status %d on yield\n", current->status);
            proc_enqueue(&run_queue[1], current);
        }

        sched_block(current);
    }
}

void sched_sleep(struct pcb *pcb, uint64_t deadline) {
    uint64_t curr_ticks = mtime_get();
    uint64_t curr_ns = curr_ticks * 1000000000 / time_base;

    if (deadline <= curr_ns) return;

    pcb->status = PROC_SLEEPING;
    pcb->deadline = deadline;

    kprintf("Start sleep for proc %p, until ddl %u\n", pcb, deadline);

    struct pcb *current = sched_self();
    if (current == pcb) sched_yield();
}
