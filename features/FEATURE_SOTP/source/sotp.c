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

#include "sotp.h"

#include "sotp_os_wrapper.h"
#include "sotp_int_flash_wrapper.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

// --------------------------------------------------------- Definitions ----------------------------------------------------------

#define TRACE_GROUP                     "sotp"

#define PR_ERR printf
#define PR_INFO printf
#define PR_DEBUG printf

#define DELETE_ITEM_FLAG 0x01

typedef struct {
    uint8_t  type;
    uint8_t  flags;
    uint16_t length;
    uint32_t mac;
} record_header_t __attribute__((aligned(4)));

#define FLASH_MINIMAL_PROG_UNIT 8

#define MASTER_RECORD_BLANK_FIELD_SIZE FLASH_MINIMAL_PROG_UNIT

typedef struct {
    uint16_t version;
    uint16_t reserved;
    uint32_t area_size;
} master_record_data_t __attribute__((aligned(4)));

#define MASTER_RECORD_SIZE sizeof(master_record_data_t)

#define MEDITATE_TIME_MS 100

typedef enum {
    AREA_STATE_NONE = 0,
    AREA_STATE_EMPTY,
    AREA_STATE_VALID,
} area_state_e;

#define INITIAL_CRC 0xFFFFFFFF

#ifndef SOTP_PROBE_ONLY
static int init_done = 0;
static uint32_t init_attempts = 0;
static uint8_t active_area;
static uint16_t active_area_version;
// Must be aligned to the size of native integer, otherwise atomic add may not work
static uint32_t free_space_offset __attribute__((aligned(8)));
static uint32_t offset_by_type[SOTP_MAX_TYPES];
static sotp_shared_lock_t write_lock;

#endif
static sotp_area_data_t flash_area_params[SOTP_INT_FLASH_NUM_AREAS];

// -------------------------------------------------- Local Functions Declaration ----------------------------------------------------

// -------------------------------------------------- Functions Implementation ----------------------------------------------------

sotp_result_e sotp_garbage_collection(uint8_t type, uint16_t buf_len_bytes, const uint32_t *buf);

// Safely increment an integer (depending on if we're thread safe or not)
// Parameters :
// value         - [IN]   Pointer to variable.
// size          - [IN]   Increment.
// Return        : Value after increment.
int32_t safe_increment(uint32_t* value, uint32_t increment)
{
#if SOTP_THREAD_SAFE
    return sotp_atomic_increment(value, increment);
#else
    *value += increment;
    return *value;
#endif
}

// Check whether a buffer is aligned.
// Parameters :
// buf           - [IN]   Data buffer.
// size          - [IN]   Alignment size.
// Return        : Boolean result.
static inline int is_buf_aligned(const void *buf, uint32_t size)
{
    return (((size_t) buf / size * size) == (size_t) buf);
}

// Pad an address to a specified size.
// Parameters :
// address       - [IN]   Address.
// size          - [IN]   Size.
// Return        : Padded address.
static inline uint32_t pad_addr(uint32_t address, uint32_t size)
{
    return (((address-1) / size) + 1) * size;
}

// Flash access helper functions, using area and offset notations

// Read from flash, given area and offset.
// Parameters :
// area          - [IN]   Flash area.
// offset        - [IN]   Offset in area.
// len_bytes     - [IN]   Length in bytes.
// buf           - [IN]   Data buffer.
// Return        : 0 on success. Error code otherwise.
int sotp_flash_read_area(uint8_t area, uint32_t offset, uint32_t len_bytes, uint32_t *buf)
{
    return sotp_int_flash_read(len_bytes, flash_area_params[area].address + offset, buf);
}

#ifndef SOTP_PROBE_ONLY
// Write to flash, given area and offset.
// Parameters :
// area          - [IN]   Flash area.
// offset        - [IN]   Offset in area.
// len_bytes     - [IN]   Length in bytes.
// buf           - [IN]   Data buffer.
// Return        : 0 on success. Error code otherwise.
int sotp_flash_write_area(uint8_t area, uint32_t offset, uint32_t len_bytes, const uint32_t *buf)
{
    return sotp_int_flash_write(len_bytes, flash_area_params[area].address + offset, buf);
}

// Erase a flash area, given area.
// Parameters :
// area          - [IN]   Flash area.
// Return        : 0 on success. Error code otherwise.
int sotp_flash_erase_area(uint8_t area)
{
    return sotp_int_flash_erase(flash_area_params[area].address, flash_area_params[area].size);
}
#endif

// CRC32 calculation. Supports "rolling" calculation (using the initial value).
// Parameters :
// init_crc      - [IN]   Initial CRC.
// data_len      - [IN]   Buffer's data length.
// data_buf      - [IN]   Data buffer.
// Return        : CRC.
static uint32_t crc32(uint32_t init_crc, uint32_t data_len, uint8_t *data_buf)
{
    uint32_t i, j;
    uint32_t crc, mask;

    crc = init_crc;
    for (i = 0; i < data_len; i++) {
       crc = crc ^ (uint32_t) (data_buf[i]);
       for (j = 0; j < 8; j++) {
          mask = -(crc & 1);
          crc = (crc >> 1) ^ (0xEDB88320 & mask);
       }
    }
    return crc;
}

// Scan start of latest continuous empty area in flash area.
// Parameters :
// area          - [IN]   Flash area.
// offset        - [OUT]  Blank chunk offset.
// Return        : 0 on success. Error code otherwise.
static int calc_empty_space(uint8_t area, uint32_t *offset)
{
    uint32_t buf[32];
    uint8_t *chbuf;
    uint32_t i, j;
    int ret;

    *offset = flash_area_params[area].size;
    for (i = 0; i < flash_area_params[area].size / sizeof(buf); i++) {
        *offset -= sizeof(buf);
        ret = sotp_flash_read_area(area, *offset, sizeof(buf), buf);
        if (ret)
            return ret;
        chbuf = (uint8_t *) buf;
        for (j = sizeof(buf); j > 0; j--) {
            if (chbuf[j-1] != SOTP_BLANK_FLASH_VAL) {
                *offset += j;
                return 0;
            }
        }
    }
    return 0;
}

// Read a record from a given area and offset.
// Parameters :
// area          - [IN]   Flash area.
// offset        - [IN]   Record offset.
// buf_len_bytes - [IN]   Length of user buffer in byte.
// buf           - [IN]   User buffer.
// actual_len_bytes
//               - [Out]  Actual length of returned data.
// validate_only - [IN]   Just validate (don't return user data).
// valid         - [Out]  Is record valid.
// type          - [Out]  Record type.
// flags         - [Out]  Record flags.
// next_offset   - [Out]  If valid, offset of next record.
// Return        : SOTP_SUCCESS on success. Error code otherwise.
static sotp_result_e read_record(uint8_t area, uint32_t offset, uint16_t buf_len_bytes, uint32_t *buf,
                                 uint16_t *actual_len_bytes, int validate_only, int *valid,
                                 uint8_t *type, uint8_t *flags, uint32_t *next_offset)
{
    uint32_t int_buf[32];
    uint32_t *buf_ptr;
    uint32_t data_len, chunk_len;
    int os_ret;
    record_header_t header;
    uint32_t crc = INITIAL_CRC;

    *valid = 1;

    os_ret = sotp_flash_read_area(area, offset, sizeof(header), (uint32_t *) &header);
    if (os_ret) {
        return SOTP_READ_ERROR;
    }

    crc = crc32(crc, sizeof(header) - sizeof(header.mac), (uint8_t *) &header);

    *actual_len_bytes = 0;
    *type = header.type;
    *flags = header.flags;

    if ((*type >= SOTP_MAX_TYPES) && (*type != SOTP_MASTER_RECORD_TYPE)) {
        *valid = 0;
        return SOTP_SUCCESS;
    }

    data_len = header.length;
    offset += sizeof(header);

    // In case of validate only enabled, we use our internal buffer for data reading,
    // instead of the user one. This allows us to use a smaller buffer, on which CRC
    // is continuously calculated.
    if (validate_only) {
        buf_ptr = int_buf;
        buf_len_bytes = sizeof(int_buf);
    }
    else {
        if (data_len > buf_len_bytes) {
            offset += data_len;
            *actual_len_bytes = data_len;
            *next_offset = pad_addr(offset, FLASH_MINIMAL_PROG_UNIT);
            return SOTP_BUFF_TOO_SMALL;
        }
        buf_ptr = buf;
    }

    while (data_len) {
        chunk_len = SOTP_MIN(data_len, buf_len_bytes);
        os_ret = sotp_flash_read_area(area, offset, chunk_len, buf_ptr);
        if (os_ret) {
            return SOTP_READ_ERROR;
        }
        crc = crc32(crc, chunk_len, (uint8_t *) buf_ptr);
        data_len -= chunk_len;
        offset += chunk_len;
    }

    if (header.mac != crc) {
        *valid = 0;
        return SOTP_SUCCESS;
    }

    *actual_len_bytes = header.length;
    *next_offset = pad_addr(offset, FLASH_MINIMAL_PROG_UNIT);

    return SOTP_SUCCESS;
}

#ifndef SOTP_PROBE_ONLY
// Write a record in a given area and offset.
// Parameters :
// area          - [IN]   Flash area.
// offset        - [IN]   Record offset.
// type          - [IN]   Record type.
// flags         - [IN]   Record flags
// data_len      - [IN]   Record's data length.
// data_buf      - [IN]   Record's data buffer.
// next_offset   - [Out]  offset of next record.
// Return        : SOTP_SUCCESS on success. Error code otherwise.
static sotp_result_e write_record(uint8_t area, uint32_t offset, uint8_t type, uint8_t flags,
                                  uint32_t data_len, const uint32_t *data_buf, uint32_t *next_offset)
{
    record_header_t header;
    uint32_t crc = INITIAL_CRC;
    int os_ret;
    uint32_t write_len;

    header.type = type;
    header.flags = flags;
    header.length = data_len;
    header.mac = 0; // Satisfy compiler
    crc = crc32(crc, sizeof(header) - sizeof(header.mac), (uint8_t *) &header);
    if (data_len)
        crc = crc32(crc, data_len, (uint8_t *) data_buf);
    header.mac = crc;

    os_ret = sotp_flash_write_area(area, offset, sizeof(header), (uint32_t *)&header);
    if (os_ret) {
        return SOTP_WRITE_ERROR;
    }

    if (data_len) {
        offset += sizeof(header);
        write_len = data_len;
        os_ret = sotp_flash_write_area(area, offset, write_len, data_buf);
        if (os_ret) {
            return SOTP_WRITE_ERROR;
        }
        offset += data_len;
    }

    *next_offset = pad_addr(offset, FLASH_MINIMAL_PROG_UNIT);
    return SOTP_SUCCESS;
}

// Write a master record in a given area.
// Parameters :
// area          - [IN]   Flash area.
// version       - [IN]   Version.
// next_offset   - [Out]  offset of next record.
// Return        : SOTP_SUCCESS on success. Error code otherwise.
static sotp_result_e write_master_record(uint8_t area, uint16_t version, uint32_t *next_offset)
{
    master_record_data_t master_rec;

    master_rec.version = version;
    master_rec.area_size = flash_area_params[area].size;
    master_rec.reserved = 0;
    return write_record(area, 0, SOTP_MASTER_RECORD_TYPE, 0, sizeof(master_rec),
                        (uint32_t*) &master_rec, next_offset);
}

// Copy a record from a given area and offset to another offset in the other area.
// Parameters :
// from_area     - [IN]   Flash area to copy from.
// from_offset   - [IN]   Record offset in current area.
// to_offset     - [IN]   Record offset in new area.
// next_offset   - [Out]  Offset of next record in the new area.
// Return        : SOTP_SUCCESS on success. Error code otherwise.
static sotp_result_e copy_record(uint8_t from_area, uint32_t from_offset, uint32_t to_offset,
                                 uint32_t *next_offset)
{
    uint32_t int_buf[32];
    uint32_t data_len, chunk_len;
    int os_ret;
    record_header_t header;

    // This function assumes that the source record is valid, so no need to recalculate CRC.

    os_ret = sotp_flash_read_area(from_area, from_offset, sizeof(header), (uint32_t *) &header);
    if (os_ret) {
        return SOTP_READ_ERROR;
    }

    data_len = header.length;

    // No need to copy records whose flags indicate deletion
    if (header.flags && DELETE_ITEM_FLAG) {
        *next_offset = pad_addr(to_offset, FLASH_MINIMAL_PROG_UNIT);
        return SOTP_SUCCESS;
    }

    // no need to align record size here, as it won't change the outcome of this condition
    if (to_offset + sizeof(header) + data_len >= flash_area_params[1-from_area].size) {
        return SOTP_FLASH_AREA_TOO_SMALL;
    }

    os_ret = sotp_flash_write_area(1-from_area, to_offset, sizeof(header), (uint32_t *)&header);
    if (os_ret) {
        return SOTP_WRITE_ERROR;
    }

    from_offset += sizeof(header);
    to_offset += sizeof(header);

    while (data_len) {
        chunk_len = SOTP_MIN(data_len, sizeof(int_buf));
        os_ret = sotp_flash_read_area(from_area, from_offset, chunk_len, int_buf);
        if (os_ret) {
            return SOTP_READ_ERROR;
        }
        os_ret = sotp_flash_write_area(1-from_area, to_offset, chunk_len, int_buf);
        if (os_ret) {
            return SOTP_WRITE_ERROR;
        }

        data_len -= chunk_len;
        from_offset += chunk_len;
        to_offset += chunk_len;
    }

    *next_offset = pad_addr(to_offset, FLASH_MINIMAL_PROG_UNIT);
    return SOTP_SUCCESS;
}

// Perform the garbage collection process.
// Parameters :
// type          - [IN]   Item's type.
// buf_len_bytes - [IN]   Item length in bytes.
// buf           - [IN]   Pointer to user buffer.
// Return      : SOTP_SUCCESS on success. Error code otherwise.
sotp_result_e sotp_garbage_collection(uint8_t type, uint16_t buf_len_bytes, const uint32_t *buf)
{
    uint32_t curr_offset, new_area_offset, next_offset;
    uint8_t curr_area;
    sotp_result_e ret;

    new_area_offset = sizeof(record_header_t) + sizeof(master_record_data_t);

    // If GC is triggered by a set item request, we need to first write that item in the new location,
    // otherwise we may either write it twice (if already included), or lose it in case we decide
    // to skip it at garbage collection phase (and the system crashes).
    if (type != SOTP_NO_TYPE) {
        ret = write_record(1 - active_area, new_area_offset, type, 0, buf_len_bytes, buf, &next_offset);
        if (ret != SOTP_SUCCESS) {
            PR_ERR("sotp_garbage_collection: write_record failed with ret 0x%x\n", ret);
            return ret;
        }
        offset_by_type[type] = new_area_offset | (1-active_area) << (sizeof(offset_by_type[type])*8 - 1);
        new_area_offset = next_offset;
    }

    // Now iterate on all types, and copy the ones who have valid offsets (meaning that they exist)
    // to the other area.
    for (type = 0; type < SOTP_MAX_TYPES; type++) {
        curr_offset = offset_by_type[type];
        curr_area = (uint8_t) (curr_offset >> (sizeof(curr_offset)*8 - 1));
        curr_offset &= ~(1 << (sizeof(curr_offset)*8 - 1));
        if ((!curr_offset) || (curr_area != active_area))
            continue;
        ret = copy_record(curr_area, curr_offset, new_area_offset, &next_offset);
        if (ret != SOTP_SUCCESS) {
            PR_ERR("sotp_garbage_collection: copy_record failed with ret 0x%x\n", ret);
            return ret;
        }
        offset_by_type[type] = new_area_offset | (1-curr_area) << (sizeof(offset_by_type[type])*8 - 1);
        new_area_offset = next_offset;
    }

    // Now write master record, with version incremented by 1.
    active_area_version++;
    ret = write_master_record(1 - active_area, active_area_version, &next_offset);
    if (ret != SOTP_SUCCESS) {
        PR_ERR("sotp_garbage_collection: write_master_record failed with ret 0x%x\n", ret);
        return ret;
    }

    free_space_offset = new_area_offset;

    // Only now we can switch to the new active area
    active_area = 1 - active_area;

    // The older area doesn't concern us now. Erase it now.
    if (sotp_flash_erase_area(1 - active_area)) {
        return SOTP_WRITE_ERROR;
    }

    return ret;
}


// Get API logics helper function. Serves both Get & Get item size APIs.
// Parameters :
// type             - [IN]   Item's type.
// buf_len_bytes    - [IN]   Item length in bytes.
// buf              - [IN]   Pointer to user buffer.
// actual_len_bytes - [OUT]  Actual length of returned data.
// validate_only    - [IN]   Just validate (don't return user data).
// Return      : SOTP_SUCCESS on success. Error code otherwise.
static sotp_result_e sotp_do_get(uint8_t type, uint16_t buf_len_bytes, uint32_t *buf, uint16_t *actual_len_bytes,
                          int validate_only)
{
    sotp_result_e ret = SOTP_SUCCESS;
    uint32_t record_offset, next_offset;
    uint8_t area, read_type, flags;
    int valid;

    if (!init_done) {
        ret = sotp_init();
        if (ret != SOTP_SUCCESS) {
            return ret;
        }
    }

    if (type >= SOTP_MAX_TYPES) {
        return SOTP_BAD_VALUE;
    }


    if (!buf)
        buf_len_bytes = 0;

    if (buf_len_bytes && !is_buf_aligned(buf, sizeof(uint32_t))) {
        return SOTP_BUFF_NOT_ALIGNED;
    }

    // This loop is required for the case we try to perform reading while GC is in progress.
    // If so, we have the following cases:
    // 1. Record is still in the older area. It will be successfully read.
    // 2. Record was already copied to the new area. Now the offset_by_type indicates it.
    // So we have two cases here:
    // a. Read from new area succeeds. Everything's OK.
    // b. Read fails (either physically or CRC error). So we know that if the area taken
    //    from offset_by_type is different from the active area, a GC is in progress, so
    //    retry the operation.
    for (;;) {
        record_offset = offset_by_type[type];
        if (!record_offset) {
            return SOTP_NOT_FOUND;
        }

        area = (uint8_t) (record_offset >> (sizeof(record_offset)*8 - 1));
        record_offset &= ~(1 << (sizeof(record_offset)*8 - 1));

        ret = read_record(area, record_offset, buf_len_bytes, buf,
                          actual_len_bytes, validate_only, &valid,
                          &read_type, &flags, &next_offset);
        if ((ret == SOTP_SUCCESS) && valid)
            break;
        // In case area is the same as expected, GC is not in progress and we have a genuine error.
        if (area == active_area) {
            if (ret == SOTP_SUCCESS) {
                ret = SOTP_DATA_CORRUPT;
            }
            PR_ERR("sotp_do_get: read_record failed with ret 0x%x\n", ret);
            return ret;
        }
    }

    return SOTP_SUCCESS;
}

// Start of API functions

sotp_result_e sotp_get(uint8_t type, uint16_t buf_len_bytes, uint32_t *buf, uint16_t *actual_len_bytes)
{
    return sotp_do_get(type, buf_len_bytes, buf, actual_len_bytes, 0);
}

sotp_result_e sotp_get_item_size(uint8_t type, uint16_t *actual_len_bytes)
{
    return sotp_do_get(type, 0, NULL, actual_len_bytes, 1);
}

static sotp_result_e sotp_do_set(uint8_t type, uint16_t buf_len_bytes, const uint32_t *buf,
                          int ignore_otp, uint8_t flags)
{
    sotp_result_e ret = SOTP_SUCCESS;
    uint32_t record_offset, record_size, new_free_space;
    uint32_t next_offset;
    uint8_t save_active_area;

    if (!init_done) {
        ret = sotp_init();
        if (ret != SOTP_SUCCESS) {
            return ret;
        }
    }

    if (type >= SOTP_MAX_TYPES) {
        return SOTP_BAD_VALUE;
    }

    if (!buf)
        buf_len_bytes = 0;

    if (buf_len_bytes && !is_buf_aligned(buf, sizeof(uint32_t))) {
        return SOTP_BUFF_NOT_ALIGNED;
    }

    if (flags && DELETE_ITEM_FLAG && !offset_by_type[type]) {
        return SOTP_NOT_FOUND;
    }

    // writers do not lock each other exclusively, but can operate in parallel.
    // Shared lock is in order to prevent GC from operating (which uses exclusive lock).
    if (sotp_sh_lock_shared_lock(write_lock)) {
        PR_ERR("sotp_set: sotp_sh_lock_shared_lock failed\n");
        return SOTP_OS_ERROR;
    }

    save_active_area = active_area;
    record_size = pad_addr(sizeof(record_header_t) + buf_len_bytes, FLASH_MINIMAL_PROG_UNIT);

    // Parallel operation of writers is allowed due to this atomic operation. This operation
    // produces an offset on which each writer can work separately, without being interrupted
    // by the other writer. The only mutual resource here is free_space_offset - which
    // gets the correct value because of this atomic increment.
    new_free_space = safe_increment(&free_space_offset, record_size);
    record_offset = new_free_space - record_size;

    // If we cross the area limit, we need to invoke GC. However, we should consider all the cases
    // where writers work in parallel, and we only want the FIRST writer to invoke GC.
    if (new_free_space >= flash_area_params[active_area].size) {
        // In the case we have crossed the limit, but the initial offset was still before the limit, this
        // means we are the first writer (common case). Exclusively lock write_lock, and invoke GC.
        if (record_offset < flash_area_params[active_area].size) {
            if (sotp_sh_lock_promote(write_lock)) {
                return SOTP_OS_ERROR;
            }
            ret = sotp_garbage_collection((uint8_t)type, buf_len_bytes, buf);
            sotp_sh_lock_exclusive_release(write_lock);
            return ret;
        }
        else {
            // In the case we have crossed the limit, and the initial offset was also after the limit,
            // this means we are not first writer (uncommon case). Just wait for GC to complete.
            // then write record.
            if (sotp_sh_lock_shared_release(write_lock)) {
                PR_ERR("sotp_set: sotp_sh_lock_shared_release failed\n");
                return SOTP_OS_ERROR;
            }
            for (;;) {
                if (sotp_sh_lock_shared_lock(write_lock)) {
                    PR_ERR("sotp_set: sotp_sh_lock_shared_lock failed\n");
                    return SOTP_OS_ERROR;
                }
                if (save_active_area != active_area) {
                    break;
                }
                if (sotp_sh_lock_shared_release(write_lock)) {
                    PR_ERR("sotp_set: sotp_sh_lock_shared_lock failed\n");
                    return SOTP_OS_ERROR;
                }
            }
            new_free_space = safe_increment(&free_space_offset, record_size);
            record_offset = new_free_space - free_space_offset;
        }
    }

    // Now write the record
    ret = write_record(active_area, record_offset, (uint8_t)type, flags, buf_len_bytes, buf, &next_offset);
    if (ret != SOTP_SUCCESS) {
        PR_ERR("sotp_set: write_record failed with err code 0x%x\n", ret);
        sotp_sh_lock_shared_release(write_lock);
        return ret;
    }

    // Update offset_by_type. High bit indicates area.
    if (flags && DELETE_ITEM_FLAG)
        offset_by_type[type] = 0;
    else
        offset_by_type[type] = record_offset | (active_area << (sizeof(offset_by_type[type])*8 - 1));

    if (sotp_sh_lock_shared_release(write_lock)) {
        PR_ERR("sotp_set: sotp_sh_lock_shared_release failed\n");
        return SOTP_OS_ERROR;
    }

    return SOTP_SUCCESS;
}

sotp_result_e sotp_set(uint8_t type, uint16_t buf_len_bytes, const uint32_t *buf)
{
    return sotp_do_set(type, buf_len_bytes, buf, 0, 0);
}

#ifdef SOTP_TESTING

sotp_result_e sotp_set_for_testing(uint8_t type, uint16_t buf_len_bytes, const uint32_t *buf)
{
    return sotp_do_set(type, buf_len_bytes, buf, 1, 0);
}

sotp_result_e sotp_remove(uint8_t type)
{
    return sotp_do_set(type, 0, NULL, 1, DELETE_ITEM_FLAG);
}
#endif

sotp_result_e sotp_init(void)
{
    uint8_t area;
    uint8_t type;
    uint8_t flags;
    int os_ret;
    sotp_result_e ret = SOTP_SUCCESS;
    uint32_t init_attempts_val;
    area_state_e area_state[SOTP_INT_FLASH_NUM_AREAS] = { AREA_STATE_NONE, AREA_STATE_NONE };
    uint32_t free_space_offset_of_area[SOTP_INT_FLASH_NUM_AREAS] = { 0, 0 };
    uint16_t versions[SOTP_INT_FLASH_NUM_AREAS] = { 0, 0 };
    uint32_t next_offset;
    master_record_data_t master_rec;
    uint16_t actual_len_bytes;
    int valid;

    if (init_done)
        return SOTP_SUCCESS;

    // This handles the case that init function is called by more than one thread concurrently.
    // Only the one who gets the value of 1 in init_attempts_val will proceed, while others will
    // wait until init is finished.
    init_attempts_val = safe_increment(&init_attempts, 1);
    if (init_attempts_val != 1) {
#if SOTP_THREAD_SAFE
        while(!init_done)
            sotp_delay(MEDITATE_TIME_MS);
#endif
        return SOTP_SUCCESS;
    }

    memset(offset_by_type, 0, sizeof(offset_by_type));

    if (!(write_lock = sotp_sh_lock_create())) {
        PR_ERR("sotp_init: sotp_sh_lock_create failed\n");
        ret = SOTP_OS_ERROR;
        goto init_end;
    }

    sotp_int_flash_init();
    for (area = 0; area < SOTP_INT_FLASH_NUM_AREAS; area++) {
        os_ret = sotp_int_flash_get_area_info(area, &flash_area_params[area]);
        if (os_ret) {
            PR_ERR("sotp_init: sotp_int_flash_get_area_info failed with err code 0x%lx\n",
                    (unsigned long) os_ret);
            ret = SOTP_OS_ERROR;
            goto init_end;
        }

        // Find start of empty space at the end of the area. This serves for both
        // knowing whether the area is empty and for the record traversal at the end.
        os_ret = calc_empty_space(area, &(free_space_offset_of_area[area]));
        if (os_ret) {
            PR_ERR("sotp_init: calc_empty_space failed with err code 0x%lx\n",
                    (unsigned long) os_ret);
            ret = SOTP_READ_ERROR;
            goto init_end;
        }

        if (!free_space_offset_of_area[area]) {
            area_state[area] = AREA_STATE_EMPTY;
            continue;
        }

        // Check validity of master record
        ret = read_record(area, 0, sizeof(master_rec), (uint32_t *) &master_rec,
                          &actual_len_bytes, 0, &valid,
                          &type, &flags, &next_offset);
        if (ret != SOTP_SUCCESS) {
            if (ret == SOTP_BUFF_TOO_SMALL) {
                // Buf too small error means that we have a corrupt master record -
                // treat it as such
                valid = 0;
            }
            else {
                PR_ERR("sotp_init: read_record failed with err code 0x%x\n", ret);
                goto init_end;
            }
        }

        // We have a non valid master record, in a non-empty area. Just erase the area.
        if ((!valid) || (type != SOTP_MASTER_RECORD_TYPE)) {
            os_ret = sotp_flash_erase_area(area);
            if (os_ret) {
                PR_ERR("sotp_init: sotp_flash_erase_area failed with err code 0x%lx\n",
                        (unsigned long) os_ret);
                ret = SOTP_WRITE_ERROR;
                goto init_end;
            }
            area_state[area] = AREA_STATE_EMPTY;
            continue;
        }
        versions[area] = master_rec.version;

        // Place free_space_offset after the master record (for the traversal,
        // which takes place after this loop).
        free_space_offset = next_offset;
        area_state[area] = AREA_STATE_VALID;

        // Unless both areas are valid (a case handled later), getting here means
        // that we found our active area.
        active_area = area;
        active_area_version = versions[area];
    }

    // In case we have two empty areas, arbitrarily assign 0 to the active one.
    if ((area_state[0] == AREA_STATE_EMPTY) && (area_state[1] == AREA_STATE_EMPTY)) {
        active_area = 0;
        ret = write_master_record(active_area, 1, &free_space_offset);
        goto init_end;
    }

    // In case we have two valid areas, choose the one having the higher version (or 0
    // in case of wrap around). Erase the other one.
    if ((area_state[0] == AREA_STATE_VALID) && (area_state[1] == AREA_STATE_VALID)) {
        if ((versions[0] > versions[1]) || (!versions[0]))
            active_area = 0;
        else
            active_area = 1;
        active_area_version = versions[active_area];
        os_ret = sotp_flash_erase_area(1 - active_area);
        if (os_ret) {
            PR_ERR("sotp_init: sotp_flash_erase_area failed with err code 0x%lx\n",
                    (unsigned long) os_ret);
            ret = SOTP_WRITE_ERROR;
            goto init_end;
        }
    }

    // Traverse area until reaching the empty space at the end or until reaching a faulty record
    while (free_space_offset < free_space_offset_of_area[active_area]) {
        ret = read_record(active_area, free_space_offset, 0, NULL,
                          &actual_len_bytes, 1, &valid,
                          &type, &flags, &next_offset);
        if (ret != SOTP_SUCCESS) {
            PR_ERR("sotp_init: read_record failed with err code 0x%x\n", ret);
            goto init_end;
        }
        // In case we have a faulty record, this probably means that the system crashed when written.
        // Perform a garbage collection, to make the the other area valid.
        if (!valid) {
            ret = sotp_garbage_collection(SOTP_NO_TYPE, 0, NULL);
            break;
        }
        if (flags && DELETE_ITEM_FLAG)
            offset_by_type[type] = 0;
        else
            offset_by_type[type] = free_space_offset | (active_area << (sizeof(offset_by_type[type])*8 - 1));
        free_space_offset = next_offset;
    }

init_end:
    init_done = 1;
    return ret;
}

sotp_result_e sotp_deinit(void)
{
    if (init_done) {
        sotp_sh_lock_destroy(write_lock);
        sotp_int_flash_deinit();
    }

    init_attempts = 0;
    init_done = 0;

    return SOTP_SUCCESS;
}

sotp_result_e sotp_reset(void)
{
    uint8_t area;
    int os_ret;

    // Erase both areas, and reinitialize the module. This is totally not thread safe,
    // as init doesn't take the case of re-initialization into account. It's OK, as this function
    // should only be called in pre-production cases.
    for (area = 0; area < SOTP_INT_FLASH_NUM_AREAS; area++) {
        if (!init_done) {
            os_ret = sotp_int_flash_get_area_info(area, &flash_area_params[area]);
            if (os_ret)
                return SOTP_OS_ERROR;
        }
        os_ret = sotp_flash_erase_area(area);
        if (os_ret)
            return SOTP_WRITE_ERROR;
    }

    sotp_deinit();
    return sotp_init();
}

#ifdef SOTP_TESTING

sotp_result_e sotp_force_garbage_collection(void)
{
    sotp_result_e ret;

    if (!init_done) {
        ret = sotp_init();
        if (ret != SOTP_SUCCESS)
            return ret;
    }

    if (sotp_sh_lock_exclusive_lock(write_lock)) {
        PR_ERR("sotp_force_garbage_collection: sotp_sh_lock_exclusive_lock failed");
        return SOTP_OS_ERROR;
    }
    ret = sotp_garbage_collection(SOTP_NO_TYPE, 0, NULL);
    sotp_sh_lock_exclusive_release(write_lock);
    return ret;
}

#endif // SOTP_PROBE_ONLY

#if defined(SOTP_PROBE_ONLY) || defined(SOTP_TESTING)
sotp_result_e sotp_probe(uint8_t type, uint16_t buf_len_bytes, uint32_t *buf, uint16_t *actual_len_bytes)
{

    uint8_t area;
    int sel_area = -1;
    uint8_t read_type;
    uint8_t flags;
    int os_ret;
    sotp_result_e ret = SOTP_SUCCESS, save_ret = SOTP_SUCCESS;
    uint32_t free_space_offset_of_area = 0;
    uint32_t curr_offset = 0, next_offset;
    master_record_data_t master_rec;
    uint16_t prev_version = 0;
    uint16_t tmp_actual_len_bytes;
    int valid;
    int found = 0;

    for (area = 0; area < SOTP_INT_FLASH_NUM_AREAS; area++) {
        os_ret = sotp_int_flash_get_area_info(area, &flash_area_params[area]);
        if (os_ret) {
            PR_ERR("sotp_probe_type: sotp_int_flash_get_area_info failed with err code 0x%lx\n",
                    (unsigned long) os_ret);
            return SOTP_OS_ERROR;
        }

        // Check validity of master record
        ret = read_record(area, 0, sizeof(master_rec), (uint32_t *) &master_rec,
                          actual_len_bytes, 0, &valid,
                          &read_type, &flags, &next_offset);
        if (ret != SOTP_SUCCESS) {
            if (ret == SOTP_BUFF_TOO_SMALL) {
                // Buf too small error means that we have a corrupt master record -
                // treat it as such, move to next area.
                continue;
            }
            else {
                PR_ERR("sotp_probe_type: read_record failed with err code 0x%x\n", ret);
                return ret;
            }
        }

        // We have a non valid master record, move to next area.
        if ((!valid) || (read_type != SOTP_MASTER_RECORD_TYPE)) {
            continue;
        }

        // Use similar logic of init's way of handling two valid areas (without erasing them of course)
        if ((area == 1) && (sel_area > 0)) {
            if ((!prev_version) || (prev_version > master_rec.version)) {
                // leave selected area as 0
                break;
            }
        }

        prev_version = master_rec.version;
        curr_offset = next_offset;
        sel_area = area;
    }

    if (sel_area < 0) {
        return SOTP_NOT_FOUND;
    }

    area = (uint8_t) sel_area;
    os_ret = calc_empty_space(area, &free_space_offset_of_area);
    if (os_ret) {
        PR_ERR("sotp_probe_type: calc_empty_space failed with err code 0x%lx\n",
                (unsigned long) os_ret);
        return SOTP_READ_ERROR;
    }

    // Traverse area until reaching the empty space at the end or until reaching a faulty record
    found = 0;
    while (curr_offset < free_space_offset_of_area) {
        // first just verify, then read to user buffer
        ret = read_record(area, curr_offset, 0, NULL,
                          &tmp_actual_len_bytes, 1, &valid,
                          &read_type, &flags, &next_offset);
        if (ret != SOTP_SUCCESS) {
            PR_ERR("sotp_probe_type: read_record failed with err code 0x%x\n", ret);
            return ret;
        }
        if (!valid) {
            break;
        }

        if (read_type == type) {
            if (flags && DELETE_ITEM_FLAG) {
                found = 0;
            }
            else {
                save_ret = read_record(area, curr_offset, buf_len_bytes, buf,
                                  actual_len_bytes, 0, &valid,
                                  &read_type, &flags, &next_offset);
                found = 1;
            }
        }
        curr_offset = next_offset;
    }

    if (!found) {
        return SOTP_NOT_FOUND;
    }

    return save_ret;
}
#endif

#endif
