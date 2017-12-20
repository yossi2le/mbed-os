/*
 * Copyright (c) 2016 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



// ----------------------------------------------------------- Includes -----------------------------------------------------------

#include "mbed.h"
#include "flash_api.h"

#include "sotp_os_wrapper.h"
#include "sotp_int_flash_wrapper.h"

#include <string.h>
#include <stdlib.h>

// --------------------------------------------------------- Definitions ----------------------------------------------------------

#define TRACE_GROUP                     "sotp"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PAGE_SIZE   16

static FlashIAP flash;

#ifndef SOTP_INT_FLASH_AREA_1_ADDRESS
#define SOTP_INT_FLASH_AREA_1_ADDRESS   0xFE000
#endif

#ifndef SOTP_INT_FLASH_AREA_1_SIZE
#define SOTP_INT_FLASH_AREA_1_SIZE      0x1000
#endif

#ifndef SOTP_INT_FLASH_AREA_2_ADDRESS
#define SOTP_INT_FLASH_AREA_2_ADDRESS   0xFF000
#endif

#ifndef SOTP_INT_FLASH_AREA_2_SIZE
#define SOTP_INT_FLASH_AREA_2_SIZE      0x1000
#endif

static size_t get_page_size(void)
{
    return SOTP_MIN(flash.get_page_size(), MAX_PAGE_SIZE);
}

static size_t get_sector_size(uint32_t address)
{
    return flash.get_sector_size(address);
}

// Program to Flash with alignments to page size
// Parameters :
// @param[in]   buffer - pointer to the buffer to be written
// @param[in]   size - the size of the buffer in bytes.
// @param[in]   address - the address of the internal flash, must be aligned to minimum writing unit (page size).
// Return     : None.
static int program_flash(size_t size, uint32_t address, const uint8_t *buffer)
{
    int ret;
    uint32_t page_size, aligned_size, remainder;
    uint8_t rem_buf[MAX_PAGE_SIZE];

    page_size = get_page_size();
    remainder = size % page_size;
    aligned_size = size - remainder;
    if (aligned_size) {
        ret = flash.program(buffer, address, aligned_size);

        if (ret) {
            return -1;
        }
        address += aligned_size;
        buffer += aligned_size;
    }

    if (!remainder) {
        return 0;
    }

    memset(rem_buf, SOTP_BLANK_FLASH_VAL, page_size);
    memcpy(rem_buf, buffer, remainder);
    ret = flash.program(buffer, address, page_size);
    if (ret) {
        return -1;
    }

    return 0;
}


int sotp_int_flash_init(void)
{
    int ret;

    ret = flash.init();
    if (ret) {
        return -1;
    }

    return 0;
}

int sotp_int_flash_deinit(void)
{
    int ret;

    ret = flash.deinit();
    if (ret) {
        return -1;
    }

    return 0;
}

int sotp_int_flash_read(size_t size, uint32_t address, uint32_t *buffer)
{
    int ret;

    if (!buffer) {
        return -1;
    }

    if (!size)
    {
        return -1;
    }
    ret = flash.read(buffer, address, size);

    if (ret) {
        return -1;
    }

    return 0;
}


int sotp_int_flash_erase(uint32_t address, size_t size)
{
    int ret;
    size_t sector_size = get_sector_size(address);

    if ((!size) || (size % sector_size) || (address % sector_size)) {
        return -1;
    }

    // No need to iterate over sectors - Flash driver's erase API does it for us
    ret = flash.erase(address, sector_size);
    if (ret) {
        return -1;
    }

    return 0;
}


int sotp_int_flash_write(size_t size, uint32_t address, const uint32_t *buffer)
{
    int ret;
    uint32_t page_size = get_page_size();
    uint32_t sector_size = get_sector_size(address);
    uint32_t chunk;
    uint8_t *buf = (uint8_t *) buffer;

    if ((!size) || (!buffer) || (address % page_size)) {
        return -1;
    }

    while (size)
    {
        chunk = SOTP_MIN(sector_size - (address % sector_size), size);
        ret = program_flash(chunk, address, buf);
        if (ret) {
            return -1;
        }
        size -= chunk;
        address += chunk;
        buf += chunk;
        sector_size = get_sector_size(address);
    }

    return 0;
}


int sotp_int_flash_get_area_info(uint8_t area, sotp_area_data_t *data)
{
    const sotp_area_data_t int_flash_areas[] =
    {
            {SOTP_INT_FLASH_AREA_1_ADDRESS, SOTP_INT_FLASH_AREA_1_SIZE},
            {SOTP_INT_FLASH_AREA_2_ADDRESS, SOTP_INT_FLASH_AREA_2_SIZE}
    };

    if ((!data) || (area > SOTP_INT_FLASH_NUM_AREAS))
    {
        return -1;
    }

    data->address = int_flash_areas[area].address;
    data->size = int_flash_areas[area].size;
    return 0;
}

#ifdef __cplusplus
}
#endif

