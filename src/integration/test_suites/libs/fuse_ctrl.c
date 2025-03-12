// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 Western Digital Corporation or its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <stdlib.h>
#include <stdint.h>

#include "soc_address_map.h"
#include "printf.h"
#include "soc_ifc.h"
#include "caliptra_ss_lc_ctrl_address_map.h"
#include "riscv_hw_if.h"
#include "fuse_ctrl_address_map.h"

void wait_dai_op_idle(uint32_t status_mask) {
    uint32_t status;
    VPRINTF(LOW, "DEBUG: Waiting for DAI to become idle...\n");
    do {
        status = lsu_read_32(FUSE_CTRL_STATUS);
    } while (((status >> FUSE_CTRL_STATUS_DAI_IDLE_OFFSET) & 0x1) == 0);
    // Clear the IDLE bit from the status value
    status &= ((((uint32_t)1) << (FUSE_CTRL_STATUS_DAI_IDLE_OFFSET - 1)) - 1);
    if (status != status_mask) {
        VPRINTF(LOW, "ERROR: unexpected status: expected: %08X actual: %08X\n", status_mask, status);
        exit(1);
    }
    VPRINTF(LOW, "DEBUG: DAI is now idle.\n");
    return;
}

void initialize_otp_controller(void) {
    uint32_t status;

    // Step 1: Check OTP controller initialization status
    VPRINTF(LOW, "DEBUG: Checking OTP controller initialization status...\n");
    status = lsu_read_32(FUSE_CTRL_STATUS);

    // Check for error bits in the status register
    if (status & 0x3FFFFF != 0 ) { // Mask all bits except DAI_IDLE
        VPRINTF(LOW, "ERROR: OTP controller initialization failed. STATUS: 0x%08X\n", status);
        return;
    }

    wait_dai_op_idle(0);

    VPRINTF(LOW, "INFO: OTP controller successfully initialized. STATUS: 0x%08X\n", status);

    // Step 2: Set up periodic background checks
    VPRINTF(LOW, "DEBUG: Configuring periodic background checks...\n");
    
    // Configure CHECK_TIMEOUT
    uint32_t check_timeout = 0x100000; // Example value, adjust as needed
    lsu_write_32(FUSE_CTRL_CHECK_TIMEOUT, check_timeout);
    VPRINTF(LOW, "INFO: CHECK_TIMEOUT set to 0x%08X\n", check_timeout);

    // Configure INTEGRITY_CHECK_PERIOD
    uint32_t integrity_check_period = 0x3FFFF; // Example value, adjust as needed
    lsu_write_32(FUSE_CTRL_INTEGRITY_CHECK_PERIOD, integrity_check_period);
    VPRINTF(LOW, "INFO: INTEGRITY_CHECK_PERIOD set to 0x%08X\n", integrity_check_period);

    // Configure CONSISTENCY_CHECK_PERIOD
    uint32_t consistency_check_period = 0x3FFFF; // Example value, adjust as needed
    lsu_write_32(FUSE_CTRL_CONSISTENCY_CHECK_PERIOD, consistency_check_period);
    VPRINTF(LOW, "INFO: CONSISTENCY_CHECK_PERIOD set to 0x%08X\n", consistency_check_period);

    // Step 3: Lock down background check registers
    VPRINTF(LOW, "DEBUG: Locking background check registers...\n");
    lsu_write_32(FUSE_CTRL_CHECK_REGWEN, 0x0);
    VPRINTF(LOW, "INFO: CHECK_REGWEN locked.\n");
}

#define FUSE_CTRL_CMD_DAI_WRITE 0x2
#define FUSE_CTRL_CMD_DAI_READ  0x1

void dai_wr(uint32_t addr, uint32_t wdata0, uint32_t wdata1, uint32_t granularity, uint32_t exp_status) {
    VPRINTF(LOW, "DEBUG: Starting DAI write operation...\n");

    //wait_dai_op_idle(0);

    VPRINTF(LOW, "DEBUG: Writing wdata0: 0x%08X to DIRECT_ACCESS_WDATA_0.\n", wdata0);
    lsu_write_32(FUSE_CTRL_DIRECT_ACCESS_WDATA_0, wdata0);

    if (granularity == 64) {
        VPRINTF(LOW, "DEBUG: Writing wdata1: 0x%08X to DIRECT_ACCESS_WDATA_1.\n", wdata1);
        lsu_write_32(FUSE_CTRL_DIRECT_ACCESS_WDATA_1, wdata1);
    }

    VPRINTF(LOW, "DEBUG: Writing address: 0x%08X to DIRECT_ACCESS_ADDRESS.\n", addr);
    lsu_write_32(FUSE_CTRL_DIRECT_ACCESS_ADDRESS, addr);

    VPRINTF(LOW, "DEBUG: Triggering DAI write command.\n");
    lsu_write_32(FUSE_CTRL_DIRECT_ACCESS_CMD, FUSE_CTRL_CMD_DAI_WRITE);

    wait_dai_op_idle(exp_status);
    return;
}

void dai_rd(uint32_t addr, uint32_t* rdata0, uint32_t* rdata1, uint32_t granularity, uint32_t exp_status) {
    VPRINTF(LOW, "DEBUG: Starting DAI read operation...\n");

    //wait_dai_op_idle(0);

    VPRINTF(LOW, "DEBUG: Writing address: 0x%08X to DIRECT_ACCESS_ADDRESS.\n", addr);
    lsu_write_32(FUSE_CTRL_DIRECT_ACCESS_ADDRESS, addr);

    VPRINTF(LOW, "DEBUG: Triggering DAI read command.\n");
    lsu_write_32(FUSE_CTRL_DIRECT_ACCESS_CMD, FUSE_CTRL_CMD_DAI_READ);

    wait_dai_op_idle(exp_status);

    *rdata0 = lsu_read_32(FUSE_CTRL_DIRECT_ACCESS_RDATA_0);
    VPRINTF(LOW, "DEBUG: Read data from DIRECT_ACCESS_RDATA_0: 0x%08X\n", *rdata0);

    if (granularity == 64) {
        *rdata1 = lsu_read_32(FUSE_CTRL_DIRECT_ACCESS_RDATA_1);
        VPRINTF(LOW, "DEBUG: Read data from DIRECT_ACCESS_RDATA_1: 0x%08X\n", *rdata1);
    }
    return;
}
