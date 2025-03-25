#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

#include "soc_address_map.h"
#include "printf.h"
#include "riscv_hw_if.h"
#include "soc_ifc.h"
#include "fuse_ctrl_address_map.h"
#include "caliptra_ss_lc_ctrl_address_map.h"
#include "caliptra_ss_lib.h"
#include "fuse_ctrl.h"
#include "lc_ctrl.h"

volatile char* stdout = (char *)SOC_MCI_TOP_MCI_REG_DEBUG_OUT;
#ifdef CPT_VERBOSITY
    enum printf_verbosity verbosity_g = CPT_VERBOSITY;
#else
    enum printf_verbosity verbosity_g = LOW;
#endif

/**
 * Program two fuses in `VENDOR_HASHES_PROD_PARTITION`, then verify whether
 * the volatile lock works as intended. The test proceeds in the following steps:
 * 
 *   1. Write a value to first fuse.
 *   2. Write a value to second fuse.
 *   3. Activate the volatile lock such that the fuse from step 2 is now in the
 *      locked region.
 *   4. Verify that writing to the second fuse now results in an error.
 */
void program_vendor_hashes_prod_partition(void) {

    // 0x6C2: CPTRA_CORE_VENDOR_PK_HASH_3
    // 0x724: CPTRA_CORE_VENDOR_PK_HASH_5
    const uint32_t addresses[2] = {0x6C2, 0x724};

    const uint32_t data = 0xdeadbeef;

    // Step 1
    dai_wr(addresses[0], data, 0, 32, 0);

    // Step 2
    dai_wr(addresses[1], data+1, 0, 32, 0);

    // Step 3
    lsu_write_32(FUSE_CTRL_VENDOR_PK_HASH_VOLATILE_LOCK, 4); // Lock all hashes starting from index 5.

    // Step 4
    dai_wr(addresses[1], data+2, 0, 32, FUSE_CTRL_STATUS_DAI_ERROR_MASK);
}

void main (void) {
    VPRINTF(LOW, "=================\nMCU Caliptra Boot Go\n=================\n\n")
    
    // Writing to Caliptra Boot GO register of MCI for CSS BootFSM to bring Caliptra out of reset 
    // This is just to see CSSBootFSM running correctly
    lsu_write_32(SOC_MCI_TOP_MCI_REG_CPTRA_BOOT_GO, 1);
    VPRINTF(LOW, "MCU: Writing MCI SOC_MCI_TOP_MCI_REG_CPTRA_BOOT_GO\n");

    uint32_t cptra_boot_go = lsu_read_32(SOC_MCI_TOP_MCI_REG_CPTRA_BOOT_GO);
    VPRINTF(LOW, "MCU: Reading SOC_MCI_TOP_MCI_REG_CPTRA_BOOT_GO %x\n", cptra_boot_go);
    
    lcc_initialization();
    // Set AXI user ID to MCU.
    grant_mcu_for_fc_writes(); 

    transition_state(TEST_UNLOCKED0, raw_unlock_token[0], raw_unlock_token[1], raw_unlock_token[2], raw_unlock_token[3], 1);
    wait_dai_op_idle(0);

    initialize_otp_controller();

    program_vendor_hashes_prod_partition();

    for (uint8_t ii = 0; ii < 160; ii++) {
        __asm__ volatile ("nop"); // Sleep loop as "nop"
    }

    SEND_STDOUT_CTRL(0xff);
}