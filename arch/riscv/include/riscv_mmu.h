/*
* Created for ECE595, ASE, Spring 2025
*
*
*/
#ifndef MMU_H_
#define MMU_H_

#define PAGE_SIZE 4096  // 4 KiB page size for Sv32
#define PTE_SIZE 4      // Each PTE (Page Table Entry) is 4 bytes

// Define bit flags for PTE entries
#define PTE_VALID    (1 << 0)  // Marks entry as valid
#define PTE_READ     (1 << 1)  // Allows read access
#define PTE_WRITE    (1 << 2)  // Allows write access
#define PTE_EXEC     (1 << 3)  // Allows execute access
#define PTE_GLOBAL   (1 << 5)  // Makes the mapping global (not ASID-specific)

#define SV32_PTE_PPN_SHIFT  12  // Physical Page Number (PPN) shift (aligns to 4 KiB)
#define SV32_PTE_PPN_MASK   0xFFFFF000 // Mask for PPN extraction
#define SV32_PTE_PPN_POS    10  // Position of PPN in PTE (Sv32 stores it at bits 31-10)

// Macros to extract page table indices from a virtual address
#define L1_INDEX(va)  (((va) >> 22) & 0x3FF)  // Level 1 (root) index from VPN[1]
#define L0_INDEX(va)  (((va) >> 12) & 0x3FF)  // Level 0 (leaf) index from VPN[0]

// Functions to be used by Zephyr
void z_riscv_mm_init(void);




#endif //MMU_H_