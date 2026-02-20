#include "syslib.h"
#include "malloc.h"
#include "thread.h"
#include "ctx.h"

struct thread {
    struct thread *next;    // queue management
	char *stack;            // stack space
    void *sp;       		// stack pointer
	void (*f)(void *arg);	// entry point
	void *arg;				// argument to entry point
    uint64_t deadline;      // for sleeping threads
};
static struct thread *thread_current, *thread_runqueue;
static struct thread *thread_sleepers, *thread_zombies;

static void thread_enqueue(struct thread **q, struct thread *t) {
    if (*q == 0)
        t->next = t;
    else {
        t->next = (*q)->next;
        (*q)->next = t;
    }
    *q = t;       // queue points to tail.  tail points to head.
}

static struct thread *thread_dequeue(struct thread **q) {
    struct thread *t = (*q)->next;    // get the head
    if (t == t->next) *q = 0;         // queue now empty
    else (*q)->next = t->next;        // tail points to new head
    return t;
}

void thread_init() {
	thread_current = calloc(1, sizeof(*thread_current));
}

static void thread_cleanup() {
    struct thread *t;

    while ((t = thread_zombies) != 0) {
        thread_zombies = t->next;
		free(t->stack);
		free(t);
	}
}

void thread_create(void (*f)(void *), void *arg, unsigned int stack_size) {
	// Put the current process on the run queue.
    struct thread *old = thread_current;
	thread_enqueue(&thread_runqueue, old);

	thread_current = calloc(1, sizeof(*thread_current));
	thread_current->stack = malloc(stack_size);
	thread_current->f = f;
	thread_current->arg = arg;

	// Start the new thread.
	ctx_start(&old->sp, &thread_current->stack[stack_size]);
	thread_cleanup();
}

static void thread_block() {
    if (thread_sleepers != 0) {
        struct thread *t = thread_sleepers;
        uint64_t now = user_gettime();
        while (thread_runqueue == 0 && now < t->deadline) {
            user_sleep(t->deadline);
            now = user_gettime();
        }
        while ((t = thread_sleepers) != 0 && t->deadline <= now) {
            thread_sleepers = t->next;
            thread_enqueue(&thread_runqueue, t);
        }
    }

    // assert thread_runqueue == 0 => thread_sleepers == 0
    if (thread_runqueue == 0) user_exit();

    struct thread *old = thread_current;
    thread_current = thread_dequeue(&thread_runqueue);
    if (thread_current != old) {
        ctx_switch(&old->sp, thread_current->sp);
        thread_cleanup();
    }
}

void thread_yield() {
	thread_enqueue(&thread_runqueue, thread_current);
    thread_block();
}

void thread_sleep(uint64_t deadline) {
    thread_current->deadline = deadline;
    struct thread **pt, *t;
    for (pt = &thread_sleepers;; pt = &t->next)
        if ((t = *pt) == 0 || t->deadline > thread_current->deadline) {
            thread_current->next = *pt;
            *pt = thread_current;
            thread_block();
            return;
        }
}

int thread_get() {
    for (;;) {
        int c = user_get(thread_runqueue == 0 && thread_sleepers == 0);
        if (c != USER_GET_NO_INPUT) return c;
        thread_yield();
    }
}

void thread_exit() {
    thread_current->next = thread_zombies;
    thread_zombies = thread_current;
	thread_block();
}

void exec_user(void) {
	(*thread_current->f)(thread_current->arg);
	thread_exit();
}

struct sema {
	unsigned int count;
	struct thread *waiting;
};

struct sema *sema_create(unsigned int count) {
    struct sema *sema = malloc(sizeof(*sema));
	sema->count = count;
    sema->waiting = 0;
    return sema;
}

void sema_inc(struct sema *sema) {
    if (sema->waiting == 0)
		sema->count++;
	else {
		struct thread *t = thread_dequeue(&sema->waiting);
        thread_enqueue(&thread_runqueue, t);
	}
}

void sema_dec(struct sema *sema) {
	if (sema->count == 0) {
        thread_enqueue(&sema->waiting, thread_current);
		thread_block();
	}
	else
		sema->count--;
}

void sema_release(struct sema *sema) {
    free(sema);
}
