/*
 * Unit test app entry: prints the unity test menu, which pytest-embedded
 * (dut.run_all_single_board_cases) drives over the QEMU UART console.
 */
#include <stdio.h>

#include "unity.h"

void app_main(void) {
    printf("\n=== PillDispenser unit tests (esp32s3) ===\n");
    unity_run_menu();
}
