#include "vms.h"

#include <assert.h>

int expected_exit_status() { return 0; }

void test() {
    vms_init(); // from pages.c

    void* l2 = vms_new_page(); // from pages.c
    void* l1 = vms_new_page(); // from pages.c
    void* l0 = vms_new_page(); // from pages.c
    void* p0 = vms_new_page(); // from pages.c

    void* virtual_address = (void*) 0xABC123;
    uint64_t* l2_entry = vms_page_table_pte_entry(l2, virtual_address, 2); // from page_table.c: 0x000
    vms_pte_set_ppn(l2_entry, vms_page_to_ppn(l1)); // from pte.c & page_table.c
    vms_pte_valid_set(l2_entry); // from pte.c

    uint64_t* l1_entry = vms_page_table_pte_entry(l1, virtual_address, 1); // from page_table.c
    vms_pte_set_ppn(l1_entry, vms_page_to_ppn(l0)); // from pte.c & page_table.c
    vms_pte_valid_set(l1_entry); // from pte.c

    uint64_t* l0_entry = vms_page_table_pte_entry(l0, virtual_address, 0); // from page_table.c
    vms_pte_set_ppn(l0_entry, vms_page_to_ppn(p0)); // from pte.c & page_table.c
    vms_pte_valid_set(l0_entry); // from pte.c
    vms_pte_read_set(l0_entry); // from pte.c
    vms_pte_write_set(l0_entry); // from pte.c

    vms_set_root_page_table(l2); // from mmu.c
    vms_write(virtual_address, 1); // from mmu.c

    void* forked_l2 = vms_fork_copy(); // from vms.c
    vms_set_root_page_table(forked_l2); // from mmu.c
    assert(l2 != forked_l2); // R1. l2 and the forked l2 should not be the same
    assert(vms_get_used_pages() == 8); // R2. Total used pages are 4 * 2
    assert(vms_read(virtual_address) == 1); // R3. The stored value in virtual address is the same

    vms_write(virtual_address, 2); // from mmu.c
    assert(vms_read(virtual_address) == 2); // R4. The stored value in the virtual address
                                            // in forked l2 should be modified
    vms_set_root_page_table(l2);
    assert(vms_read(virtual_address) == 1); // R5. The stored value in the virtual address in l2 should be the same
}
