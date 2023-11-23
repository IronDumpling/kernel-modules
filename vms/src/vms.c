#include "vms.h"

#include "mmu.h"
#include "pages.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static int ref_num[MAX_PAGES] = {0};

void error_handler(int num, char * msg){
    if(num <= 0){
        perror(msg);
        exit(errno);
    }
}

void entry_copy(uint64_t* src, uint64_t* dst, void* pointer){
    vms_pte_set_ppn(dst, vms_page_to_ppn(pointer));

    if(vms_pte_custom(src)) vms_pte_custom_set(dst);
    else vms_pte_custom_clear(dst);

    if(vms_pte_write(src)) vms_pte_write_set(dst);
    else vms_pte_write_clear(dst);

    if(vms_pte_read(src)) vms_pte_read_set(dst);
    else vms_pte_read_clear(dst);

    if(vms_pte_valid(src)) vms_pte_valid_set(dst);
    else vms_pte_valid_clear(dst);
}

void* vms_fork_copy() {
    error_handler(vms_get_used_pages() <= MAX_PAGES/2, "max pages count reaches");

    // copy l2 page table
    void * src_page_l2 = vms_get_root_page_table();
    void * dst_page_l2 = vms_new_page();
    for(int i = 0; i < NUM_PTE_ENTRIES; i++){
        uint64_t * src_entry_l2 = vms_page_table_pte_entry_from_index(src_page_l2, i);
        uint64_t * dst_entry_l2 = vms_page_table_pte_entry_from_index(dst_page_l2, i);
        if(!vms_pte_valid(src_entry_l2)) continue;

        void * dst_page_l1 = vms_new_page();
        entry_copy(src_entry_l2, dst_entry_l2, dst_page_l1);

        // copy l1 page table
        void * src_page_l1 = vms_ppn_to_page(vms_pte_get_ppn(src_entry_l2));
        for(int j = 0; j < NUM_PTE_ENTRIES; j++){
            uint64_t * src_entry_l1 = vms_page_table_pte_entry_from_index(src_page_l1, j);
            uint64_t * dst_entry_l1 = vms_page_table_pte_entry_from_index(dst_page_l1, j);
            if(!vms_pte_valid(src_entry_l1)) continue;

            void * dst_page_l0 = vms_new_page();
            entry_copy(src_entry_l1, dst_entry_l1, dst_page_l0);

            // copy l0 page table
            void * src_page_l0 = vms_ppn_to_page(vms_pte_get_ppn(src_entry_l1));
            for(int k = 0; k < NUM_PTE_ENTRIES; k++){
                uint64_t * src_entry_l0 = vms_page_table_pte_entry_from_index(src_page_l0, k);
                uint64_t * dst_entry_l0 = vms_page_table_pte_entry_from_index(dst_page_l0, k);
                if(!vms_pte_valid(src_entry_l0)) continue;

                void * dst_page_p = vms_new_page();
                void * src_page_p = vms_ppn_to_page(vms_pte_get_ppn(src_entry_l0));
                entry_copy(src_entry_l0, dst_entry_l0, dst_page_p);

                // copy page
                memcpy(dst_page_p, src_page_p, PAGE_SIZE);
            }
        }
    }

    return dst_page_l2;
}

void page_fault_handler(void* virtual_address, int level, void* page_table) {
    uint64_t* entry = vms_page_table_pte_entry(page_table, virtual_address, level);

    if(!vms_pte_valid(entry)) return;

    if(!vms_pte_write(entry)) {
        int idx = vms_get_page_index(vms_ppn_to_page(vms_pte_get_ppn(entry)));
        if(ref_num[idx] <= 0)
            return;
        if(ref_num[idx] == 1){
            if(vms_pte_custom(entry)){
                vms_pte_write_set(entry);
                vms_pte_custom_clear(entry);
            }
            return;
        }

        // rewrite the l0 page entry
        void * dst_page_p = vms_new_page();
        void * src_page_p = vms_ppn_to_page(vms_pte_get_ppn(entry));
        vms_pte_set_ppn(entry, vms_page_to_ppn(dst_page_p));
        if(vms_pte_custom(entry)){
            vms_pte_write_set(entry);
            vms_pte_custom_clear(entry);
        }

        // copy page
        memcpy(dst_page_p, src_page_p, PAGE_SIZE);
        ref_num[vms_get_page_index(src_page_p)]--;
        ref_num[vms_get_page_index(dst_page_p)]++;
    }
}

void* vms_fork_copy_on_write() {
    error_handler(vms_get_used_pages() <= MAX_PAGES/2, "max pages count reaches");

    // copy l2 page table
    void * src_page_l2 = vms_get_root_page_table();
    void * dst_page_l2 = vms_new_page();
    for(int i = 0; i < NUM_PTE_ENTRIES; i++){
        uint64_t * src_entry_l2 = vms_page_table_pte_entry_from_index(src_page_l2, i);
        uint64_t * dst_entry_l2 = vms_page_table_pte_entry_from_index(dst_page_l2, i);
        if(!vms_pte_valid(src_entry_l2)) continue;

        void * dst_page_l1 = vms_new_page();
        entry_copy(src_entry_l2, dst_entry_l2, dst_page_l1);

        // copy l1 page table
        void * src_page_l1 = vms_ppn_to_page(vms_pte_get_ppn(src_entry_l2));
        for(int j = 0; j < NUM_PTE_ENTRIES; j++){
            uint64_t * src_entry_l1 = vms_page_table_pte_entry_from_index(src_page_l1, j);
            uint64_t * dst_entry_l1 = vms_page_table_pte_entry_from_index(dst_page_l1, j);
            if(!vms_pte_valid(src_entry_l1)) continue;

            void * dst_page_l0 = vms_new_page();
            entry_copy(src_entry_l1, dst_entry_l1, dst_page_l0);

            // copy l0 page table
            void * src_page_l0 = vms_ppn_to_page(vms_pte_get_ppn(src_entry_l1));
            for(int k = 0; k < NUM_PTE_ENTRIES; k++){
                uint64_t * src_entry_l0 = vms_page_table_pte_entry_from_index(src_page_l0, k);
                uint64_t * dst_entry_l0 = vms_page_table_pte_entry_from_index(dst_page_l0, k);
                if(!vms_pte_valid(src_entry_l0)) continue;
                void * page_p = vms_ppn_to_page(vms_pte_get_ppn(src_entry_l0));

                entry_copy(src_entry_l0, dst_entry_l0, page_p);
                if(vms_pte_write(src_entry_l0)){
                    vms_pte_write_clear(dst_entry_l0);
                    vms_pte_custom_set(dst_entry_l0);
                    vms_pte_write_clear(src_entry_l0);
                    vms_pte_custom_set(src_entry_l0);
                }
                ref_num[vms_get_page_index(page_p)] += 2;
            }
        }
    }

    return dst_page_l2;
}
