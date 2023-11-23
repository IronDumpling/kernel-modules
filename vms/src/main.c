#include "vms.h"

#include <stdio.h>

int main() {
    vms_init(); // from pages.c

    void* l2 = vms_new_page(); // from pages.c
    void* l1 = vms_new_page(); // from pages.c
    void* l0 = vms_new_page(); // from pages.c
    
    void* p0 = vms_new_page(); // from pages.c

    void* virtual_address = (void*) 0xABC123;

    uint64_t* l2_entry = vms_page_table_pte_entry(l2, virtual_address, 2); // from page_table.c
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
    printf("0x%lX read: %d\n",
           (uint64_t) virtual_address,
           vms_read(virtual_address)); // from mmu.c
    vms_write(virtual_address, 2);
    printf("0x%lX read: %d\n",
           (uint64_t) virtual_address,
           vms_read(virtual_address)); // from mmu.c

    return 0;
}
