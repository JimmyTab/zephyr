/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #include <stdio.h>

// int main(void)
// {
// 	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

// 	return 0;
// }

/*----------------- Includes ---------------*/
#include <stdint.h>
#include <string.h>


#include <riscv_mmu.h>
#include <zephyr/sys/util.h>


int main () {

    for (int i=0; i < 0xFFFF ; i += KB(4)) {
        riscv_map_page((uintptr_t)&i,(uintptr_t)&i,0);
    }
}
