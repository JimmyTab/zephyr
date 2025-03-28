/*
* Created for ECE595, ASE, Spring 2025
* Kallie, Pranav, Alex, & James
*
*/


/*----------------- Includes ---------------*/
#include <zephyr/kernel.h>
#include <zephyr/arch/riscv/csr.h>
#include <zephyr/sys/mem_manage.h>
#include <string.h>

#include <riscv_mmu.h>

/*---------------- External Variables ----------*/
// Linker Symbols for Kernel Memory Regions
// These are defined in Zephyr's linker script and provide addresses of kernel memory regions
extern uintptr_t _image_ram_start;
extern uintptr_t _image_ram_end;
extern uintptr_t _kernel_text_start;
extern uintptr_t _kernel_text_end;

/*---------------- Global Variables -----------*/
typedef uint32_t riscv_pte_t; // A single 32-bit Sv32 Page Table Entry (PTE)
static struct riscv_mmu_l1_page_table l1_page_table __aligned(KB(4)) = {0}; // Pointer to the root page table (Level 1)

/*----------------- PFP ----------------------*/
void riscv_tlb_flush_all(void);
void riscv_tlb_flush(uintptr_t);

/*----------------- Static Functions --------*/

/**
 * @brief Allocates a 4 KiB-aligned page table
 * @return A pointer to the allocated page table, or NULL on failure
 */
static void *allocate_l2_page_table(void)
{
    // Allocate slightly more than PAGE_SIZE to allow for alignment
    void *ptr  = k_malloc(PAGE_SIZE); // Allocate extra memory
    if (!ptr) {
        printk("MMU: Failed to allocate page table\n");
        return NULL;
    }

    // Align the pointer to the next PAGE_SIZE boundary
    return ptr;
}



/*----------------- Public Functions ----------*/

/**
 * @brief Initializes the MMU and sets up Sv32 page tables
 */
void z_riscv_mm_init(void)
{
    struct riscv_mmu_l2_page_table * l2_page_table;
    /* 2. Identity Mapping for Kernel Memory */
    // Get physical and virtual addresses for the kernel
    uintptr_t kernel_start_phys = (uintptr_t)&_kernel_text_start;
    uintptr_t kernel_end_phys = (uintptr_t)&_kernel_text_end;
    uintptr_t kernel_virt = (uintptr_t)&_kernel_text_start;

    // Ensure physical addresses are page-aligned
    kernel_start_phys = kernel_start_phys & SV32_PTE_PPN_MASK;
    kernel_end_phys = (kernel_end_phys + PAGE_SIZE - 1) & SV32_PTE_PPN_MASK;

    printk("MMU: Kernel physical start: %p, end: %p, virtual: %p\n",
           (void *)kernel_start_phys, (void *)kernel_end_phys, (void *)kernel_virt);

    uintptr_t phys = kernel_start_phys;
    uintptr_t virt = kernel_virt;

    // Iterate through kernel pages and set up mappings
    while (phys < kernel_end_phys) {
        uint32_t l1_index = L1_INDEX(virt);
        uint32_t l2_index = L2_INDEX(virt);

        /* Allocate Level 2 Page Table if Not Already Present */
        if (l1_page_table.entries[l1_index].page_table_entry.v != 1) {
            l2_page_table =(struct riscv_mmu_l2_page_table *)allocate_l2_page_table();
            if (!l2_page_table) {
                printk("MMU: Failed to allocate Level 0 page table\n");
                return;
            }

            // Clear the Level 2 page table (L2)
            memset(l2_page_table, 0, PAGE_SIZE);
            printk("MMU: Allocated Level 0 page table at %p for L1 index %d\n", l2_page_table, l1_index);

            // Store Level 2 page table address in Level 1 entry
            l1_page_table.entries[l1_index].l2_page_table_ref.l2_page_table_address = ((uintptr_t)l2_page_table >> SV32_PT_L2_ADDR_SHIFT) & SV32_PT_L2_ADDR_MASK;
        }

        /* Get Level 2 Page Table Address */
        l2_page_table =(struct riscv_mmu_l2_page_table *) (l1_page_table.entries[l1_index].word & (SV32_PT_L2_ADDR_MASK << SV32_PT_L2_ADDR_SHIFT));

        /* Map the Virtual Address to Physical Address */
        l2_page_table->entries[l2_index].l2_page_4k.pa_base = (phys >> SV32_PTE_PPN_SHIFT) & SV32_PT_L2_ADDR_MASK;


        printk("MMU: Mapped VA %p -> PA %p (L1 Index %d, L2 Index %d)\n", 
               (void *)virt, (void *)phys, l1_index, l2_index);

        /* Move to the Next Page */
        phys += PAGE_SIZE;
        virt += PAGE_SIZE;
    }

    /* 3. Set `satp` Register to Enable MMU */
    uintptr_t root_pte_ppn = ((uintptr_t)&l1_page_table) >> SV32_PTE_PPN_SHIFT; // Get root page table’s PPN
    uintptr_t satp_value = (1UL << 31) | root_pte_ppn; // Sv32 mode + root page table

    __asm__ volatile ("csrw satp, %0" :: "r" (satp_value));

    printk("MMU: Enabled Sv32, SATP = 0x%lx\n", satp_value);

    /* 4. Flush the Entire TLB */
    riscv_tlb_flush_all();
    printk("MMU: Flushed entire TLB (sfence.vma)\n");

    /* 5. Debug Output */
    printk("RISC-V MMU initialized: Root Page Table at %p, SATP = %lx\n", &l1_page_table, satp_value);
}

/**
 * @brief Maps a virtual address to a physical address in the RISC-V Sv32 page table.
 *
 * This function inserts a new mapping into the MMU, translating a virtual address (VA)
 * to a physical address (PA) with the given access permissions. If the required Level 0
 * page table does not exist, it will be allocated dynamically.
 *
 * @param virt  The virtual address to be mapped (must be 4 KiB aligned).
 * @param phys  The physical address to map to (must be 4 KiB aligned).
 * @param flags Access permissions (e.g., PTE_READ | PTE_WRITE | PTE_EXEC).
 *
 * @note This function assumes the root page table is already allocated and initialized.
 *       The function also flushes the TLB entry for the mapped address using `sfence.vma`.
 */
void riscv_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags)
{
    uint32_t l1_index = L1_INDEX(virt);
    uint32_t l2_index = L2_INDEX(virt);

    struct riscv_mmu_l2_page_table * l2_page_table;

    /* 1. Check if the Level 1 Page Table Exists */
    if (l1_page_table.entries[l1_index].page_table_entry.v != 1) {
        l2_page_table = (struct riscv_mmu_l2_page_table * )(allocate_l2_page_table());
        if (!l2_page_table) {
            printk("MMU: Failed to allocate Level 0 page table\n");
            return;
        }

        // Clear the Level 2 page table (L2)
        memset(l2_page_table, 0, PAGE_SIZE);
        printk("MMU: Allocated Level 0 page table at %p for L1 index %d\n", l2_page_table, l1_index);

        // Store Level 2 page table address in Level 1 entry
        l1_page_table.entries[l1_index].l2_page_table_ref.l2_page_table_address = ((uintptr_t)l2_page_table >> SV32_PT_L2_ADDR_SHIFT) & SV32_PT_L2_ADDR_MASK;
        l1_page_table.entries[l1_index].page_table_entry.v = 1;
        l1_page_table.entries[l1_index].page_table_entry.u = 1;

    }

    /* Get Level 2 Page Table Address */
    l2_page_table =(struct riscv_mmu_l2_page_table *) (l1_page_table.entries[l1_index].word & (SV32_PT_L2_ADDR_MASK << SV32_PT_L2_ADDR_SHIFT));

    /* Map the Virtual Address to Physical Address */
    l2_page_table->entries[l2_index].l2_page_4k.pa_base = (phys >> SV32_PTE_PPN_SHIFT) & SV32_PT_L2_ADDR_MASK;
    l2_page_table->entries[l2_index].l2_page_4k.v = 1;
    l2_page_table->entries[l2_index].l2_page_4k.u = 1;
    l2_page_table->entries[l2_index].l2_page_4k.x = 1;
    l2_page_table->entries[l2_index].l2_page_4k.w = 1;

    printk("MMU: Mapped VA %p -> PA %p (L1 Index %d, L2 Index %d)\n", 
            (void *)virt, (void *)phys, l1_index, l2_index);
/* 4. Flush TLB for this mapping */
    riscv_tlb_flush(virt);
}

/**
 * @brief Unmaps a virtual address from the RISC-V Sv32 page table.
 *
 * This function removes a virtual-to-physical mapping by clearing the PTE entry.
 * If the Level 0 table exists but becomes empty, it can be freed (optional).
 *
 * @param virt  The virtual address to be unmapped (must be 4 KiB aligned).
 */
void riscv_unmap_page(uintptr_t virt)
{
    uint32_t l1_index = L1_INDEX(virt);
    uint32_t l2_index = L2_INDEX(virt);

    /* 1. Check if the Level 0 Page Table Exists */
    if (l1_page_table.entries[l1_index].page_table_entry.v != 1) {
        printk("MMU: Unmap failed, no L0 table for VA %p\n", (void *)virt);
        return;
    }

    /* 2. Get the L2 Page Table Address */
    struct riscv_mmu_l2_page_table * l2_page_table = (struct riscv_mmu_l2_page_table *) (l1_page_table.entries[l1_index].word & (SV32_PT_L2_ADDR_MASK << SV32_PT_L2_ADDR_SHIFT));

    /* 3. Check if Mapping Exists */
    if (l2_page_table->entries[l2_index].l2_page_4k.v != 1) {
        printk("MMU: Unmap failed, VA %p is not mapped\n", (void *)virt);
        return;
    }

    /* 4. Remove the Mapping */
    l2_page_table->entries[l2_index].l2_page_4k.v = 0;
    printk("MMU: Unmapped VA %p (L1 Index %d, L0 Index %d)\n", (void *)virt, l1_index, l2_index);

    /* 5. Flush TLB for this address */
    riscv_tlb_flush(virt);
}

/**
 * @brief Flushes a specific virtual address from the TLB.
 *
 * Ensures that changes to page table mappings are recognized by the MMU.
 *
 * @param virt The virtual address to flush from the TLB.
 */
void riscv_tlb_flush(uintptr_t virt)
{
    printk("MMU: Flushing TLB for VA %p\n", (void *)virt);
    __asm__ volatile ("sfence.vma %0, x0" :: "r"(virt) : "memory");
    
}

/**
 * @brief Flushes the entire TLB (Translation Lookaside Buffer).
 *
 * This function ensures that all cached virtual-to-physical mappings are invalidated.
 * It is required when switching address spaces or performing global memory updates.
 */
void riscv_tlb_flush_all(void)
{
    printk("MMU: Flushing entire TLB\n");
    __asm__ volatile ("sfence.vma x0, x0" ::: "memory");
}

/**
 * @brief Maps a range of virtual addresses to physical addresses.
 *
 * This function maps a virtual memory range to a corresponding physical memory range
 * with the specified access permissions. It ensures that the mapping is page-aligned
 * and covers the full range requested.
 *
 * @param virt  The starting virtual address (must be page-aligned).
 * @param phys  The starting physical address (must be page-aligned).
 * @param size  The number of bytes to map (must be a multiple of PAGE_SIZE).
 * @param flags Access permissions (e.g., PTE_READ | PTE_WRITE | PTE_EXEC).
 *
 * @return 0 on success, -EINVAL if alignment is incorrect.
 */
int arch_mem_map(void *virt, uintptr_t phys, size_t size, uint32_t flags)
{
    printk("MMU: arch_mem_map() called - VA %p -> PA %p, size: %lu bytes, flags: 0x%x\n",
           virt, (void *)phys, size, flags);

    // Ensure addresses and size are page-aligned
    if (((uintptr_t)virt & (PAGE_SIZE - 1)) || (phys & (PAGE_SIZE - 1)) || (size & (PAGE_SIZE - 1))) {
        printk("MMU: arch_mem_map() failed - addresses must be page-aligned\n");
        return -EINVAL;
    }

    uintptr_t va = (uintptr_t)virt;
    uintptr_t pa = phys;

    // Iterate through each 4 KiB page and map it
    while (size > 0) {
        printk("MMU: Mapping VA %p -> PA %p with flags 0x%x\n", (void *)va, (void *)pa, flags);
        riscv_map_page(va, pa, flags);
        va += PAGE_SIZE;
        pa += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    printk("MMU: arch_mem_map() completed successfully.\n");
    return 0;
}


/**
 * @brief Unmaps a range of virtual addresses.
 *
 * This function removes virtual memory mappings in the specified range.
 *
 * @param virt  The starting virtual address (must be page-aligned).
 * @param size  The number of bytes to unmap (must be a multiple of PAGE_SIZE).
 *
 * @return 0 on success, -EINVAL if alignment is incorrect.
 */
int arch_mem_unmap(void *virt, size_t size)
{
    printk("MMU: arch_mem_unmap() called - VA %p, size: %lu bytes\n", virt, size);

    // Ensure virtual address and size are page-aligned
    if (((uintptr_t)virt & (PAGE_SIZE - 1)) || (size & (PAGE_SIZE - 1))) {
        printk("MMU: arch_mem_unmap() failed - addresses must be page-aligned\n");
        return -EINVAL;
    }

    uintptr_t va = (uintptr_t)virt;

    // Iterate through each 4 KiB page and unmap it
    while (size > 0) {
        printk("MMU: Unmapping VA %p\n", (void *)va);
        riscv_unmap_page(va);
        va += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    printk("MMU: arch_mem_unmap() completed successfully.\n");
    return 0;
}

/**
 * @brief Retrieves the physical address mapped to a given virtual address.
 *
 * This function looks up the MMU page tables to find the physical address (PA)
 * that corresponds to a given virtual address (VA).
 *
 * @param virt  The virtual address to look up.
 * @param phys  Pointer to store the retrieved physical address.
 *
 * @return 0 on success, -EINVAL if the virtual address is not mapped.
 */
int arch_page_phys_get(void *virt, uintptr_t *phys)
{
    uintptr_t va = (uintptr_t)virt;
    uint32_t l1_index = L1_INDEX(va);
    uint32_t l2_index = L2_INDEX(va);

    /* 1. Check if the L0 Page Table Exists */
    if (l1_page_table.entries[l1_index].page_table_entry.v != 1) {
        printk("MMU: arch_page_phys_get() failed - No L0 table for VA %p\n", virt);
        return -EINVAL;
    }

    /* 2. Get the L0 Page Table */
    /* 2. Get the L2 Page Table Address */
    struct riscv_mmu_l2_page_table * l2_page_table = (struct riscv_mmu_l2_page_table *) (l1_page_table.entries[l1_index].word);

    /* 3. Check if the Mapping Exists */
    if (l2_page_table->entries[l2_index].l2_page_4k.v != 1) {
        printk("MMU: arch_page_phys_get() failed - VA %p is not mapped\n", virt);
        return -EINVAL;
    }

    /* 4. Extract the Physical Address */
    phys = (uintptr_t)l2_page_table->entries[l2_index].l2_page_4k.pa_base << SV32_PTE_PPN_POS;

    printk("MMU: arch_page_phys_get() - VA %p -> PA %p\n", virt, (void *)*phys);

    return 0;
}

/**
 * @brief Handles a page fault by allocating and mapping a new page.
 *
 * This function is called when an unmapped virtual address is accessed. It
 * allocates a new physical page, maps it, and flushes the TLB entry.
 *
 * @param fault_addr The virtual address that caused the page fault.
 *
 * @return 0 on success, -ENOMEM if memory allocation fails, -EINVAL if fault_addr is misaligned.
 */
int riscv_handle_page_fault(uintptr_t fault_addr)
{
    // Ensure the faulting address is page-aligned
    if (fault_addr & (PAGE_SIZE - 1)) {
        printk("MMU: Page fault handler failed - misaligned address %p\n", (void *)fault_addr);
        return -EINVAL;
    }

    printk("MMU: Handling page fault at VA %p\n", (void *)fault_addr);

    // Step 2: Allocate a new physical page (for now, just get a dummy page)
    void *new_page_ptr = k_malloc(PAGE_SIZE);
    if (!new_page_ptr) {
        printk("MMU: Page fault handler failed - Out of memory\n");
        return -ENOMEM;
    }

    // Ensure the physical page is cleared
    memset(new_page_ptr, 0, PAGE_SIZE);
    uintptr_t new_phys_page = (uintptr_t)new_page_ptr;
    printk("MMU: Allocated new page at PA %p for VA %p\n", (void *)new_phys_page, (void *)fault_addr);

    // Step 3: Map the new page into the MMU
    riscv_map_page(fault_addr, new_phys_page, PTE_VALID | PTE_READ | PTE_WRITE | PTE_EXEC);

    // Step 4: Flush the TLB entry for this address
    riscv_tlb_flush(fault_addr);

    printk("MMU: Page fault resolved - VA %p -> PA %p\n", (void *)fault_addr, (void *)new_phys_page);

    return 0;
}