/*
 * test_mmu.c - MMU Tests for RISC-V Sv32
 */

 #include <zephyr/kernel.h>
 #include <zephyr/ztest.h>
 #include <zephyr/sys/printk.h>
 #include <string.h>
 #include <riscv_mmu.h>
 #include <stdint.h>
 #include <zephyr/sys/util.h>
 
 #define TEST_PAGE_VIRT_BASE 0x80000000
 #define TEST_PAGE_SIZE     PAGE_SIZE
 
 /* Helper function to read a value from a virtual address */
 static uint32_t read_virtual_address(volatile uint32_t *addr)
 {
     return *addr;
 }
 
 /* Helper function to write a value to a virtual address */
 static void write_virtual_address(volatile uint32_t *addr, uint32_t value)
 {
     *addr = value;
 }
 
 /*---------------- Identity Mapping Test ----------------*/
//  static void test_identity_mapping(void)
//  {
//      uintptr_t test_phys_addr = (uintptr_t)&_kernel_text_start; // Choose a physical address in kernel text
//      volatile uint32_t *test_virt_addr = (volatile uint32_t *)test_phys_addr;
//      uint32_t original_value;
//      uint32_t read_value;
//      uint32_t test_pattern = 0x12345678;
 
//      printk("TEST: Identity Mapping\n");
 
//      /* Ensure the test address is within the kernel identity mapping */
//      if ((uintptr_t)test_virt_addr < (uintptr_t)&_kernel_text_start || (uintptr_t)test_virt_addr >= (uintptr_t)&_kernel_text_end) {
//          ztest_test_skip("Test address not within kernel identity mapping.");
//          return;
//      }
 
//      /* Read the original value */
//      original_value = read_virtual_address(test_virt_addr);
 
//      /* Write a test pattern */
//      write_virtual_address(test_virt_addr, test_pattern);
 
//      /* Read back the value using the same (identity-mapped) virtual address */
//      read_value = read_virtual_address(test_virt_addr);
 
//      /* Assert that the read value matches the written value */
//      zassert_equal(read_value, test_pattern, "Identity mapping test failed: Read value (0x%x) != Written value (0x%x)",
//                    read_value, test_pattern);
 
//      /* Restore the original value */
//      write_virtual_address(test_virt_addr, original_value);
 
//      printk("TEST: Identity Mapping - PASSED\n");
//  }
 
 /*---------------- Permission Bits Tests ----------------*/
//  static void test_permission_read_only(void)
//  {
//      uintptr_t phys_page;
//      uintptr_t virt_addr = TEST_PAGE_VIRT_BASE + TEST_PAGE_SIZE * 0; // Allocate a virtual address
//      volatile uint32_t *test_addr = (volatile uint32_t *)virt_addr;
//      uint32_t test_pattern = 0x9ABCDEF0;
//      int result;
 
//      printk("TEST: Read-Only Permission\n");
 
//      /* Allocate a physical page */
//      void *allocated_page = k_malloc(TEST_PAGE_SIZE);
//      zassert_not_null(allocated_page, "Failed to allocate physical page.");
//      phys_page = (uintptr_t)allocated_page;
 
//      /* Map the page as read-only */
//      riscv_map_page(virt_addr, phys_page, PTE_VALID | PTE_READ);
 
//      /* Try to write to the read-only page */
//      sys_memory_barrier();
//      write_virtual_address(test_addr, test_pattern);
//      sys_memory_barrier();
 
//      /* Attempting to write should ideally trigger an exception (page fault).
//       * Since we don't have full exception handling in this test, we'll try to
//       * read back. If the write was successful (which it shouldn't be under
//       * proper MMU protection), the read value would be the test pattern. */
//      uint32_t read_value = read_virtual_address(test_addr);
//      zassert_not_equal(read_value, test_pattern, "Read-only test failed: Write was unexpectedly successful (read 0x%x, expected not 0x%x)",
//                        read_value, test_pattern);
 
//      /* Unmap the page and free the physical memory */
//      riscv_unmap_page(virt_addr);
//      k_free(allocated_page);
 
//      printk("TEST: Read-Only Permission - PASSED (assuming write fault occurred)\n");
//  }
 
//  static void test_permission_no_access(void)
//  {
//      uintptr_t phys_page;
//      uintptr_t virt_addr = TEST_PAGE_VIRT_BASE + TEST_PAGE_SIZE * 1; // Another virtual address
//      volatile uint32_t *test_addr = (volatile uint32_t *)virt_addr;
//      int result;
 
//      printk("TEST: No Access Permission\n");
 
//      /* Allocate a physical page */
//      void *allocated_page = k_malloc(TEST_PAGE_SIZE);
//      zassert_not_null(allocated_page, "Failed to allocate physical page.");
//      phys_page = (uintptr_t)allocated_page;
 
//      /* Map the page with no access permissions (only valid bit) */
//      riscv_map_page(virt_addr, phys_page, PTE_VALID);
 
//      /* Try to read from the no-access page */
//      sys_memory_barrier();
//      uint32_t read_attempt = read_virtual_address(test_addr); // This should cause a fault
//      sys_memory_barrier();
 
//      /* Try to write to the no-access page */
//      sys_memory_barrier();
//      write_virtual_address(test_addr, 0x0); // This should also cause a fault
//      sys_memory_barrier();
 
//      /* If we reach here without a fault, the test has failed.
//       * In a real system with proper exception handling, these accesses would
//       * trigger page faults. Since we don't have that here, we'll rely on the
//       * expectation that the MMU will prevent access. We can't directly assert
//       * a fault without more advanced testing infrastructure. */
//      printk("TEST: No Access Permission - PASSED (assuming access fault occurred)\n");
 
//      /* Unmap the page and free the physical memory */
//      riscv_unmap_page(virt_addr);
//      k_free(allocated_page);
//  }
 
ZTEST_SUITE(mmu_test,NULL,NULL,NULL,NULL,NULL);

ZTEST(mmu_test,init_and_map)
{
	uintptr_t ram_start = (uintptr_t)&_image_ram_start;
    uintptr_t ram_end   = (uintptr_t)&_image_ram_end;
    size_t ram_size     = ram_end - ram_start;
	uintptr_t phys_addr;
	uintptr_t virt_base_512 = 0x80000000;
    uintptr_t phys_base_512 = 0x40000000;

	uintptr_t mapped_va = virt_base_512 + KB(4);
    uintptr_t mapped_pa = phys_base_512 + KB(4);
	uintptr_t unmapped_va = virt_base_512 + KB(8);
    uintptr_t unmapped_pa = phys_base_512 + KB(8);
	int page_mapped = 0;
	int ret = 0;

	z_riscv_mm_init();
	
	// Map base page.
    riscv_map_page(virt_base_512, phys_base_512, 0);        
	zassert_ok(arch_page_phys_get((void *)virt_base_512, &phys_addr));
	zassert_not_ok(arch_page_phys_get((void *)mapped_va, &phys_addr));
	zassert_not_ok(arch_page_phys_get((void *)unmapped_va, &phys_addr));

	// Map a second page.
    riscv_map_page(mapped_va, mapped_pa, 0);        
	zassert_ok(arch_page_phys_get((void *)virt_base_512, &phys_addr));
	zassert_ok(arch_page_phys_get((void *)mapped_va, &phys_addr));
	zassert_not_ok(arch_page_phys_get((void *)unmapped_va, &phys_addr));

	// Unmap a second page.
	riscv_unmap_page(mapped_va);
	zassert_not_ok(arch_page_phys_get((void *)mapped_va, &phys_addr));
	zassert_ok(arch_page_phys_get((void *)virt_base_512, &phys_addr));
	zassert_not_ok(arch_page_phys_get((void *)unmapped_va, &phys_addr));
	
	// Unmap base page
	riscv_unmap_page(virt_base_512);
	zassert_not_ok(arch_page_phys_get((void *)mapped_va, &phys_addr));
	zassert_not_ok(arch_page_phys_get((void *)virt_base_512, &phys_addr));
	zassert_not_ok(arch_page_phys_get((void *)unmapped_va, &phys_addr));

}

ZTEST(mmu_test,bit_permission)
{
	uintptr_t ram_start = (uintptr_t)&_image_ram_start;
    uintptr_t ram_end   = (uintptr_t)&_image_ram_end;
    size_t ram_size     = ram_end - ram_start;
	uintptr_t phys_addr;
	uintptr_t virt_base_512 = 0x80000000;
    uintptr_t phys_base_512 = 0x40000000;

	z_riscv_mm_init();
}

ZTEST(mmu_test,bit_values)
{
	uintptr_t ram_start = (uintptr_t)&_image_ram_start;
    uintptr_t ram_end   = (uintptr_t)&_image_ram_end;
    size_t ram_size     = ram_end - ram_start;
	uintptr_t phys_addr;
	uintptr_t virt_base_512 = 0x80000000;
    uintptr_t phys_base_512 = 0x40000000;

	z_riscv_mm_init();
}