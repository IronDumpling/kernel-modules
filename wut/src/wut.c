#include "wut.h"

#include <assert.h> // assert
#include <errno.h> // errno
#include <stddef.h> // NULL
#include <stdio.h> // perror
#include <stdlib.h> // reallocarray
#include <sys/mman.h> // mmap, munmap
#include <sys/signal.h> // SIGSTKSZ
#include <sys/queue.h> // TAILQ_*
#include <ucontext.h> // getcontext, makecontext, setcontext, swapcontext
#include <valgrind/valgrind.h> // VALGRIND_STACK_REGISTER

/* Declarations */
#define DEBUG 0
void wut_exit(int status);
void print_threads();
void print_queue();
typedef struct thread thread;
typedef struct entry entry;

static void die(const char* message) {
    int err = errno;
    perror(message);
    exit(err);
}

/* Stack */
static char* new_stack(void) {
    char* stack = mmap(
        NULL,
        SIGSTKSZ,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if (stack == MAP_FAILED) {
        die("mmap stack failed");
    }
    VALGRIND_STACK_REGISTER(stack, stack + SIGSTKSZ);
    return stack;
}

static void delete_stack(char* stack) {
    if (munmap(stack, SIGSTKSZ) == -1) {
        die("munmap stack failed");
    }
}

/* Thread */
typedef struct thread{
    int id;
    int block_id; // the thread ID that is blocked by this thread
    int state; // -1 is alive, -2 is blocked, 0~255 is exited
    char * stack;
    entry * ent;
    ucontext_t context;
    ucontext_t * ctx_ptr;
} thread;

/* Queue */
typedef struct entry{
    thread * val;
    TAILQ_ENTRY(entry) entries;
} entry;

/* Thread Pool */
thread ** threads;
int threads_count;
int threads_capacity;

/* Ready Queue */
TAILQ_HEAD(head, entry);
static struct head ready_queue;

/* Thread Scheduler */
ucontext_t scheduler, * scheduler_ptr = &scheduler;

/* Helper Functions P1. Threads Pool */
void threads_expand(){
    int new_capacity = threads_capacity * 2;
    threads = (thread **) realloc(threads, new_capacity * sizeof(thread*));
    if(threads == NULL) die("threads re-alloc");

    for(int i = threads_capacity; i < new_capacity; i++){
        threads[i] = NULL;
    }
    threads_capacity = new_capacity;
}

int min_available_id(){
    for(int i = 0; i < threads_capacity; i++){
        if(threads[i] == NULL) return i;
    }
    return -1;
}

void print_threads(){
    printf("Current Threads TCB Array\n");
    for(int i = 0; i < threads_capacity; i++){
        if(threads[i] != NULL)
            printf("%dth thread ID: %d, block ID: %d, status: %d\n", i, threads[i]->id, threads[i]->block_id, threads[i]->state);
    }
}

/* Helper Functions P2. Thread */
thread * create_thread(void (*run)(void)){
    threads_count++;

    thread * new = (thread*) malloc(sizeof (thread));
    if(new == NULL) die("create new thread malloc");

    int id = min_available_id();
    if(id < 0) die("no available thread");

    new->id = id;
    new->block_id = -1;
    new->state = -1;
    new->stack = new_stack();

    new->ctx_ptr = &(new->context);
    getcontext(new->ctx_ptr);
    new->ctx_ptr->uc_stack.ss_sp = new->stack;
    new->ctx_ptr->uc_stack.ss_size = SIGSTKSZ;
    new->ctx_ptr->uc_stack.ss_flags = 0;
    new->ctx_ptr->uc_link = scheduler_ptr;
    if(run != NULL) makecontext(new->ctx_ptr, (void(*)()) run, 0);

    return new;
}

void delete_thread(thread * th){
//    delete_stack(th->stack);
//    th->ctx_ptr->uc_stack.ss_sp = NULL;
//    th->ctx_ptr->uc_stack.ss_size = 0;
    th->ctx_ptr->uc_stack.ss_flags = 0;
    th->ctx_ptr->uc_link = NULL;
    th->ent = NULL;
//    free(th->ctx_ptr);
//    free(th);
}

/* Helper Functions P3. Queue */
entry * create_entry(thread * th){
    entry * en = (entry*) malloc(sizeof (entry));
    if(en == NULL) die("create new entry malloc");
    en->val = th;
    th->ent = en;
    TAILQ_INSERT_TAIL(&ready_queue, en, entries);
    return en;
}

void print_queue(){
    entry * curr = NULL;
    int counter = 0;
    printf("Current Ready Queue\n");
    TAILQ_FOREACH(curr, &ready_queue, entries){
        printf("%dth entry, thread ID: %d, block ID: %d, status: %d\n", counter, curr->val->id, curr->val->block_id, curr->val->state);
        counter++;
    }
}

void schedule(){
    getcontext(scheduler_ptr);

    if(TAILQ_EMPTY(&ready_queue)) exit(0);
    entry * head = TAILQ_FIRST(&ready_queue);

    if(head->val->state < 0) wut_exit(0);

    if(DEBUG){
        printf("\nEnter Scheduler\n");
        print_threads();
        print_queue();
    }

    if(head->val->block_id >= 0){
        int wake_id = head->val->block_id;
        TAILQ_INSERT_TAIL(&ready_queue, threads[wake_id]->ent, entries);
        threads[wake_id]->state = -1;
        if(DEBUG){
            printf("\nWake Thread %d\n", wake_id);
            print_threads();
            print_queue();
        }
    }

    delete_thread(head->val);
    TAILQ_REMOVE(&ready_queue, head, entries);

    if(TAILQ_EMPTY(&ready_queue)) exit(0);
    head = TAILQ_FIRST(&ready_queue);

    if(DEBUG){
        printf("\nSchedule Thread %d To Run\n", head->val->id);
        print_threads();
        print_queue();
    }

    setcontext(head->val->ctx_ptr);
    delete_thread(head->val);
}

/* Library Functions */
void wut_init() {
    /* Threads Pool */
    threads_count = 0;
    threads_capacity = 10;
    threads = (thread**) malloc(threads_capacity * sizeof (thread*));
    if(threads == NULL) die("threads malloc");
    for(int i = 0; i < threads_capacity; i++){
        threads[i] = NULL;
    }

    /* Scheduler */
    if(getcontext(scheduler_ptr) < 0) die("scheduler get context");
    scheduler_ptr->uc_stack.ss_sp = new_stack();
    scheduler_ptr->uc_stack.ss_size = SIGSTKSZ;
    scheduler_ptr->uc_stack.ss_flags = 0;
    scheduler_ptr->uc_link = NULL;
    makecontext(scheduler_ptr, (void(*)()) schedule, 0);

    /* Main Thread */
    thread * main_thread = create_thread(NULL);
    threads[main_thread->id] = main_thread;

    /* Queue */
    TAILQ_INIT(&ready_queue);
    create_entry(main_thread);

    if(DEBUG){
        printf("Initial Thread %d\n", main_thread->id);
        print_threads();
        print_queue();
    }
}

int wut_id() {
    entry * head = TAILQ_FIRST(&ready_queue);
    return head->val->id;
}

int wut_create(void (*run)(void)) {
    if(threads_count >= threads_capacity) threads_expand();
    thread * new_thread = create_thread(run);
    threads[new_thread->id] = new_thread;
    create_entry(new_thread);

    if(DEBUG){
        printf("\nCreate new thread %d\n", new_thread->id);
        print_threads();
        print_queue();
    }

    return new_thread->id;
}

int wut_cancel(int id) {
    entry * head = TAILQ_FIRST(&ready_queue);
    if(id < 0 || id > threads_capacity || threads[id] == NULL || head->val->id == id) return -1;

    entry * curr = NULL;
    TAILQ_FOREACH(curr, &ready_queue, entries){
        if(curr->val->block_id == id){
            curr->val->block_id = -1;
            break;
        }
    }

    if(threads[id]->block_id >= 0){
        int wake_id = threads[id]->block_id;
        TAILQ_INSERT_TAIL(&ready_queue, threads[wake_id]->ent, entries);
        threads[wake_id]->state = -1;
    }

    TAILQ_REMOVE(&ready_queue, threads[id]->ent, entries);
    threads[id]->state = 128;
    delete_thread(threads[id]);

    if(DEBUG){
        printf("\nCancel thread %d\n", id);
        print_threads();
        print_queue();
    }
    return 0;
}

int wut_join(int id) {
    if(TAILQ_EMPTY(&ready_queue)) return -1;

    entry * old_head = TAILQ_FIRST(&ready_queue);
    if(id < 0 || id > threads_capacity || threads[id] == NULL || id == old_head->val->id || threads[id]->block_id >= 0) return -1;
    if(threads[id]->state >= 0){
        int state = threads[id]->state;
        threads[id] = NULL;
        return state;
    }

    old_head->val->state = -2;
    threads[id]->block_id = old_head->val->id;
    TAILQ_REMOVE(&ready_queue, old_head, entries);

    entry * new_head = TAILQ_FIRST(&ready_queue);

    if(DEBUG){
        printf("\nThread %d Join Thread %d\n", old_head->val->id, id);
        print_threads();
        print_queue();
    }

    if(swapcontext(old_head->val->ctx_ptr, new_head->val->ctx_ptr) < 0) die("yield swap context");

    if(DEBUG){
        printf("\nThread %d Join Thread %d Finished\n", old_head->val->id, id);
        print_threads();
        print_queue();
    }

    int state = threads[id]->state;
    threads[id] = NULL;
    return state;
}

int wut_yield() {
    entry * old_head = TAILQ_FIRST(&ready_queue);
    if(TAILQ_NEXT(old_head, entries) == NULL) return -1;
    TAILQ_REMOVE(&ready_queue, old_head, entries);
    TAILQ_INSERT_TAIL(&ready_queue, old_head, entries);

    if(DEBUG){
        printf("\nYield thread %d\n", old_head->val->id);
        print_queue();
    }

    entry * new_head = TAILQ_FIRST(&ready_queue);
    if(swapcontext(old_head->val->ctx_ptr, new_head->val->ctx_ptr) < 0) die("yield swap context");
    return 0;
}

// The Correct Way to Exit The Thread
// 1. 在exit和cancel中，删除thread除了status之外的所有信息
// 2. 如果有其他thread想join这个thread，那就在join结束后，删除status信息
// 3. 如果没有其他thread想join这个thread，那就在检查TCB Array时，怕断所哟欧status大于0的Entry为可以使用的Entry，并覆盖该Entry对的信息
void wut_exit(int status) {
    entry * head = TAILQ_FIRST(&ready_queue);
    status &= 0xFF;
    head->val->state = status;

    if(DEBUG){
        printf("\nExit Thread %d\n", head->val->id);
        print_threads();
        print_queue();
    }

    if(swapcontext(head->val->ctx_ptr, scheduler_ptr) < 0) die("yield swap context");
}
