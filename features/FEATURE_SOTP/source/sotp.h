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

#ifndef __SOTP_H
#define __SOTP_H

#include <stdint.h>
#include "sotp_int_flash_wrapper.h"
#include "sotp_os_wrapper.h"

#ifdef SOTP_TESTING
#undef SOTP_PROBE_ONLY
#endif

typedef enum {
    SOTP_SUCCESS                = 0,
    SOTP_READ_ERROR             = 1,
    SOTP_WRITE_ERROR            = 2,
    SOTP_NOT_FOUND              = 3,
    SOTP_DATA_CORRUPT           = 4,
    SOTP_BAD_VALUE              = 5,
    SOTP_BUFF_TOO_SMALL         = 6,
    SOTP_FLASH_AREA_TOO_SMALL   = 7,
    SOTP_OS_ERROR               = 8,
    SOTP_BUFF_NOT_ALIGNED       = 9,
    SOTP_ALREADY_EXISTS         = 10,
    SOTP_ERROR_MAXVAL           = 0xFFFF
} sotp_result_e;

#define SOTP_MASTER_RECORD_TYPE 0xFE
#define SOTP_NO_TYPE            0xFF

#ifndef SOTP_MAX_TYPES
#define SOTP_MAX_TYPES 16
#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns one item of data programmed on Flash, given type.
 *
 * @param [in] type
 *               Type of stored item.
 *
 * @param [in] buf_len_bytes
 *               Length of input buffer in bytes.
 *
 * @param [in] buf
 *               Buffer to store data on (must be aligned to a 32 bit boundary).
 *
 * @param [out] actual_len_bytes.
 *               Actual length of returned data
 *
 * @returns SOTP_SUCCESS           Value was found on Flash.
 *          SOTP_NOT_FOUND         Value was not found on Flash.
 *          SOTP_READ_ERROR        Physical error reading data.
 *          SOTP_DATA_CORRUPT      Data on Flash is corrupt.
 *          SOTP_BAD_VALUE         Bad value in any of the parameters.
 *          SOTP_BUFF_TOO_SMALL    Not enough memory in user buffer.
 *          SOTP_BUFF_NOT_ALIGNED  Buffer not aligned to 32 bits.
 */
sotp_result_e sotp_get(uint8_t type, uint16_t buf_len_bytes, uint32_t *buf, uint16_t *actual_len_bytes);

/**
 * @brief Returns one item of data programmed on Flash, given type.
 *
 * @param [in] type
 *               Type of stored item.
 *
 * @param [out] actual_len_bytes.
 *               Actual length of item
 *
 * @returns SOTP_SUCCESS           Value was found on Flash.
 *          SOTP_NOT_FOUND         Value was not found on Flash.
 *          SOTP_READ_ERROR        Physical error reading data.
 *          SOTP_DATA_CORRUPT      Data on Flash is corrupt.
 *          SOTP_BAD_VALUE         Bad value in any of the parameters.
 */
sotp_result_e sotp_get_item_size(uint8_t type, uint16_t *actual_len_bytes);


/**
 * @brief Programs one item of data on Flash, given type.
 *
 * @param [in] type
 *               Type of stored item.
 *
 * @param [in] buf_len_bytes
 *               Item length in bytes.
 *
 * @param [in] buf
 *               Buffer containing data  (must be aligned to a 32 bit boundary).
 *
 * @returns SOTP_SUCCESS           Value was successfully written on Flash.
 *          SOTP_WRITE_ERROR       Physical error writing data.
 *          SOTP_BAD_VALUE         Bad value in any of the parameters.
 *          SOTP_FLASH_AREA_TOO_SMALL
 *                                 Not enough space in Flash area.
 *          SOTP_BUFF_NOT_ALIGNED  Buffer not aligned to 32 bits.
 *          SOTP_ALREADY_EXISTS    Item (OTP type) already exists.
 *
 */
sotp_result_e sotp_set(uint8_t type, uint16_t buf_len_bytes, const uint32_t *buf);

#ifdef SOTP_TESTING
/**
 * @brief Remove an item from flash.
 *
 * @param [in] type
 *               Type of stored item.
 *
 * @param [in] buf_len_bytes
 *               Item length in bytes.
 *
 * @param [in] buf
 *               Buffer containing data  (must be aligned to a 32 bit boundary).
 *
 * @returns SOTP_SUCCESS           Value was successfully written on Flash.
 *          SOTP_WRITE_ERROR       Physical error writing data.
 *          SOTP_BAD_VALUE         Bad value in any of the parameters.
 *          SOTP_FLASH_AREA_TOO_SMALL
 *                                 Not enough space in Flash area.
 *          SOTP_BUFF_NOT_ALIGNED  Buffer not aligned to 32 bits.
 *
 */
sotp_result_e sotp_remove(uint8_t type);

/**
 * @brief Programs one item of data on Flash, given type. No OTP existing check.
 *
 * @param [in] type
 *               Type of stored item.
 *
 * @param [in] buf_len_bytes
 *               Item length in bytes.
 *
 * @param [in] buf
 *               Buffer containing data  (must be aligned to a 32 bit boundary).
 *
 * @returns SOTP_SUCCESS           Value was successfully written on Flash.
 *          SOTP_WRITE_ERROR       Physical error writing data.
 *          SOTP_BAD_VALUE         Bad value in any of the parameters.
 *          SOTP_FLASH_AREA_TOO_SMALL
 *                                 Not enough space in Flash area.
 *          SOTP_BUFF_NOT_ALIGNED  Buffer not aligned to 32 bits.
 *
 */
sotp_result_e sotp_set_for_testing(uint8_t type, uint16_t buf_len_bytes, const uint32_t *buf);
#endif

/**
 * @brief Initializes SOTP component.
 *
 * @returns SOTP_SUCCESS       Initialization completed successfully.
 *          SOTP_READ_ERROR    Physical error reading data.
 *          SOTP_WRITE_ERROR   Physical error writing data (on recovery).
 *          SOTP_FLASH_AREA_TOO_SMALL
 *                             Not enough space in Flash area.
 */
sotp_result_e sotp_init(void);

/**
 * @brief Deinitializes SOTP component.
 *        Warning: This function is not thread safe and should not be called
 *        concurrently with other SOTP functions.
 *
 * @returns SOTP_SUCCESS       Deinitialization completed successfully.
 */
sotp_result_e sotp_deinit(void);

/**
 * @brief Reset Flash SOTP areas.
 *        Warning: This function is not thread safe and should not be called
 *        concurrently with other SOTP functions.
 *
 * @returns SOTP_SUCCESS       Reset completed successfully.
 *          SOTP_READ_ERROR    Physical error reading data.
 *          SOTP_WRITE_ERROR   Physical error writing data.
 */
sotp_result_e sotp_reset(void);

#ifdef SOTP_TESTING

/**
 * @brief Initiate a forced garbage collection.
 *
 * @returns SOTP_SUCCESS       GC completed successfully.
 *          SOTP_READ_ERROR    Physical error reading data.
 *          SOTP_WRITE_ERROR   Physical error writing data.
 *          SOTP_FLASH_AREA_TOO_SMALL
 *                             Not enough space in Flash area.
 */
sotp_result_e sotp_force_garbage_collection(void);
#endif

/**
 * @brief Returns one item of data programmed on Flash, given type.
 *        This is the "initless" version of the function, traversing the flash if triggered.
 *
 * @param [in] type
 *               Type of stored item (must be between 0-15).
 *
 * @param [in] buf_len_bytes
 *               Length of input buffer in bytes.
 *
 * @param [in] buf
 *               Buffer to store data on (must be aligned to a 32 bit boundary).
 *
 * @param [out] actual_len_bytes.
 *               Actual length of returned data
 *
 * @returns SOTP_SUCCESS           Value was found on Flash.
 *          SOTP_NOT_FOUND         Value was not found on Flash.
 *          SOTP_READ_ERROR        Physical error reading data.
 *          SOTP_DATA_CORRUPT      Data on Flash is corrupt.
 *          SOTP_BAD_VALUE         Bad value in any of the parameters.
 *          SOTP_BUFF_TOO_SMALL    Not enough memory in user buffer.
 *          SOTP_BUFF_NOT_ALIGNED  Buffer not aligned to 32 bits.
 */
sotp_result_e sotp_probe(uint8_t type, uint16_t buf_len_bytes, uint32_t *buf, uint16_t *actual_len_bytes);


#ifdef __cplusplus
}
#endif

#endif


