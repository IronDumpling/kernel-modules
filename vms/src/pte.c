#include "vms.h"

/**
 * PTE: 64bits
 * Reserved + PPN + RSW + Flags (D+A+G+U+X+W+R+V)
 * PPN: 10th - 53th bits
 * CUSTOM (RSW): 8th - 9th bits
 * WRITE_FLAG: 2nd bit
 * READ_FLAG: 1st bit
 * VALID_FLAG: 0th bit
 */
#define PTE_CUSTOM (1 << 8)
#define PTE_WRITE (1 << 2)
#define PTE_READ  (1 << 1)
#define PTE_VALID (1 << 0)
#define PTE_PPN_START_BIT 10

/**
 * Set the valid flag of the given PTE to be false
 * @param entry
 */
void vms_pte_valid_clear(uint64_t* entry) {
    *entry &= ~PTE_VALID;
}

/**
 * Set the valid flag of the given PTE to be true
 * @param entry
 */
void vms_pte_valid_set(uint64_t* entry) {
    *entry |= PTE_VALID;
}

/**
 * Check the valid flag of the given PTE
 * @param entry
 */
int vms_pte_valid(uint64_t* entry) {
    return (*entry & PTE_VALID) != 0;
}

/**
 * Set the read flag of the given PTE to be false
 * @param entry
 */
void vms_pte_read_clear(uint64_t* entry) {
    *entry &= ~PTE_READ;
}

/**
 * Set the read flag of the given PTE to be true
 * @param entry
 */
void vms_pte_read_set(uint64_t* entry) {
    *entry |= PTE_READ;
}

/**
 * Check the read flag of the given PTE
 * @param entry
 */
int vms_pte_read(uint64_t* entry) {
    return (*entry & PTE_READ) != 0;
}

/**
 * Set the write flag of the given PTE to be false
 * @param entry
 */
void vms_pte_write_clear(uint64_t* entry) {
    *entry &= ~PTE_WRITE;
}

/**
 * Set the write flag of the given PTE to be true
 * @param entry
 */
void vms_pte_write_set(uint64_t* entry) {
    *entry |= PTE_WRITE;
}

/**
 * Check the write flag of the given PTE
 * @param entry
 */
int vms_pte_write(uint64_t* entry) {
    return (*entry & PTE_WRITE) != 0;
}

/**
 * Set the custom flag of the given PTE to be false
 * @param entry
 */
void vms_pte_custom_clear(uint64_t* entry) {
    *entry &= ~PTE_CUSTOM;
}

/**
 * Set the custom flag of the given PTE to be true
 * @param entry
 */
void vms_pte_custom_set(uint64_t* entry) {
    *entry |= PTE_CUSTOM;
}

/**
 * Check the custom flag of the given PTE
 * @param entry
 */
int vms_pte_custom(uint64_t* entry) {
    return (*entry & PTE_CUSTOM) != 0;
}

/**
 * vms_pte_get_ppn
 * Get the PPN of the given PTE
 * @param entry (PTE)
 * @return PPN
 */
uint64_t vms_pte_get_ppn(uint64_t* entry) {
    uint64_t mask = ((((uint64_t)~0) << 20) >> PTE_PPN_START_BIT);
    return (*entry & mask) >> PTE_PPN_START_BIT;
}

/**
 * vms_pte_set_ppn
 * Set the value of the given PTE to the given PPN value
 * @param entry
 * @param ppn
 */
void vms_pte_set_ppn(uint64_t* entry, uint64_t ppn) {
    uint64_t mask = ~((((uint64_t)~0) << 20) >> PTE_PPN_START_BIT);
    *entry &= mask;
    ppn = (ppn << 20) >> PTE_PPN_START_BIT;
    *entry |= ppn;
}
