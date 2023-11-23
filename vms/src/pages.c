#include "vms.h"

#include "pages.h"

#include <assert.h> // assert
#include <errno.h> // errno
#include <stdint.h> // uintptr_t
#include <stdio.h> // perror
#include <stdlib.h> // exit
#include <string.h> // memset
#include <sys/mman.h> // mmap
#include <unistd.h> // sysconf

static void* base_pointer = NULL;
static int allocated[MAX_PAGES] = {0}; /* This can be more space efficient */ // RAM
static int used_pages = 0;

/**
 * vms_get_page_pointer
 * @param index
 * @return the pointer of the index th page
 */
void* vms_get_page_pointer(int index) {
    return ((uint8_t*) base_pointer) + (index * PAGE_SIZE);
}

int vms_get_page_index(void* pointer) {
    return (((uint64_t) pointer) - ((uint64_t) base_pointer)) / PAGE_SIZE;
}

void check_page_aligned(void* pointer) {
    assert((uintptr_t) pointer % PAGE_SIZE == 0);
}

void vms_init() {
    assert(sysconf(_SC_PAGE_SIZE) == PAGE_SIZE);

    /**
     * mmap(void addr, size_t length, int prot, int flags, int fd, off_t offset);
     * Memory Mapping function creates a new mapping in the virtual address space of the calling process.
     * 1. addr: If addr == NULL, then the kernel chooses the page-aligned address at
     * which to create the mapping. This is the most portable method of creating a new mapping.
     * 2. length:
     * 3. prot: It describes the desired memory protection of the mapping, and must not conflict with
     * the open mode of the file. PROT_READ: Pages may be read; PROT_WRITE: Pages may be written.
     * 4. flag: It determines whether updates to the mapping are visible to other processes mapping
     * the same region, and whether updates are carried through to the underlying file.
     * MAP_SHARED: Share this mapping. Updates to the mapping are visible to other processes mapping
     * the same region, and carried through to the underlying file.
     * MAP_ANONYMOUS: The mapping is not backed by any file. Its contents are initialized to zero.
     * 5. fd: With MAP_ANONYMOUS be set, the fd argument is ignored or to be -1.
     * 6. offset: With MAP_ANONYMOUS be set, the offset argument should be 0.
     * */

    base_pointer = mmap(
        NULL,
        MAX_PAGES * PAGE_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_SHARED,
        -1, 0);

    if (base_pointer == MAP_FAILED) {
        int err = errno;
        perror("mmap");
        exit(err);
    }

    check_page_aligned(base_pointer);
}

/**
 * vms_new_page
 * create a new page at the first available place
 * @return page table pointer
 */
void* vms_new_page() {
    for (int i = 0; i < MAX_PAGES; ++i) {
        if (!allocated[i]) {
            allocated[i] = 1;
            ++used_pages;
            return vms_get_page_pointer(i);
        }
    }
    exit(ENOMEM);
}

void vms_free_page(void* pointer) {
    check_page_aligned(pointer);

    int i = vms_get_page_index(pointer);
    assert(allocated[i]);

    allocated[i] = 0;
    /**
     * memset(void *str, int c, size_t n)
     * 1. str: The pointer to the block of memory to fill.
     * 2. c: The value to be set.
     * 3. n: The number of bytes to be set to the value.
     */
    memset(pointer, 0, PAGE_SIZE);
    --used_pages;
}

/**
 * vms_get_used_pages
 * @return the used page count
 */
int vms_get_used_pages() {
    return used_pages;
}
