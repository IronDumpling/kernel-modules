#include "wut.h"
#include <stdio.h>
#include <sys/mman.h> // mmap
#include <errno.h> // errno

int* shared_memory = NULL;
#define TEST_MAGIC 0x0FF51DE5

/* Do not modify this function, you should call this to check for any value
   you want to inspect from the solution. */
void check(int value, const char* message) {
    printf("Check: %d (%s)\n", value, message);
}

void thread3(){
    /* (6) Thread 0 is added into the waiting queue
     * Thread 3 is blocked by Thread 1. Thread 1 is running.*/
    shared_memory[9] = wut_join(shared_memory[1]);
    check(shared_memory[9], "return value of thread 3 joins thread 1");
}

void thread2(){
    /* (3) Thread 2 creates Thread 3.
     * ready queue: {3}*/
    shared_memory[6] = wut_create(thread3);
    check(shared_memory[6], "id of thread 3");
    /* (4) It waits until Thread 3 exits.
     * Thread 2 is blocked and Threads starts running */
    shared_memory[7] = wut_join(shared_memory[6]);
    check(shared_memory[7], "return value of thread 2 joins thread 3");
}

void thread1(){
    /* (5) Thread 1 starts running, the waiting queue only has Thread 2.
     * Thread 2 starts running, and thread 1 is added at the end of the waiting queue. */
    shared_memory[4] = wut_yield();
    check(shared_memory[4], "return value of thread 1 yield");
}

int main() {
    /*
    You may write any of your own testing code here.

    You can execute it using `build/test/wut`. However, without your own
    implementation, it shouldn't do much. We'll run this code with the solution
    so you can clarify anything you want, or write a tricky test case.

    Place at least one call to `check` with a value (probably a return from a
    library call) that you'd like to see the output of. For example, here's
    how to convert `tests/main-thread-is-0.c` into this format:
    
    wut_init();
    check(wut_id(), "wut_id of the main thread is should be 0");

    */

    wut_init();

    shared_memory = mmap(NULL, 4096 * 10, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (shared_memory == MAP_FAILED) {
        exit(errno);
    }

    for (int i = 0; i < 10240; ++i) {
        shared_memory[i] = TEST_MAGIC;
    }

    /* (1) Create 2 extra threads,
     * thread 0 is running
     * ready queue: {1, 2} */
    shared_memory[0] = wut_id(); // thread 0
    check(shared_memory[0], "id of thread 0");
    shared_memory[1] = wut_create(thread1); // thread 1
    check(shared_memory[1], "id of thread 1");
    shared_memory[2] = wut_create(thread2); // thread 2
    check(shared_memory[2], "id of thread 2");
    /* (2) Thread 0 would wait for thread 2 to end.
     * Thread 0 is blocked and Thread 1 is running.
     * ready queue: {2} */
    shared_memory[3] = wut_join(shared_memory[2]);
    /* (7) Thread 0 would wait for thread 3 to end.
     * Thread 0 is blocked and Thread 1 is running. */
    check(shared_memory[3], "return value of thread 0 joins thread 2");
    /* (8) Thread 0 exits */
    return 0;
}
