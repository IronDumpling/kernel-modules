#include "vms.h"

#include "pages.h"

#include <assert.h>

/**
 * 64 bit system: 9 * 3 + 12 = 39 bits
 * Three level page table: L2 + L1 + L0
 * Each VPN has 9 bits, Offset has 12 bits
 */
#define INDEX_BITS 9
#define OFFSET_BITS 12

/**
 * Get the index of the virtual address at given level
 * @param virtual_address
 * @param level
 * @return index
 */
uint16_t vms_page_table_index(void* virtual_address, int level) {
    uint8_t start_bit = INDEX_BITS * level + OFFSET_BITS;
    uint64_t mask = (uint64_t) 0x1FF << start_bit;
    return (mask & ((uint64_t) virtual_address)) >> start_bit;
}

/**
 * Get pte at given index from page_table
 * @param page_table
 * @param index
 * @return pte
 */
uint64_t* vms_page_table_pte_entry_from_index(void* page_table, int index) {
    return &((uint64_t*) page_table)[index];
}

/**
 * vms_page_table_pte_entry
 * Get the pte from the given table and given virtual address
 * 1. Calculate index (VPN) from virtual address and level num
 * 2. Get the pte at index from the page table
 * @param page_table
 * @param virtual_address
 * @param level
 * @return uint64_t * pte
 */
uint64_t* vms_page_table_pte_entry(void* page_table, void* virtual_address, int level) {
    uint16_t index = vms_page_table_index(virtual_address, level);
    return vms_page_table_pte_entry_from_index(page_table, index);
}

/**
 * vms_ppn_to_page
 * get page pointer from PPN (physical page number)
 * @param ppn
 * @return Page pointer
 */
void* vms_ppn_to_page(uint64_t ppn) {
    return (void*) (ppn << OFFSET_BITS);
}

/**
 * vms_page_to_ppn
 * get PPN (physical page number) from page pointer
 * @param pointer
 * @return PPN
 */
uint64_t vms_page_to_ppn(void* pointer) {
    check_page_aligned(pointer);
    uint64_t ppn = (uint64_t) pointer;
    assert((ppn & 0xF000000000000000) == 0);
    return ppn >> OFFSET_BITS;
}
