/*
 * Copyright (c) 2018 ARM Limited. All rights reserved.
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

#ifndef MBED_KVSTORE_H
#define MBED_KVSTORE_H

#include <stdint.h>
#include <stdio.h>

typedef enum {
    KVSTORE_SUCCESS                =  0,
    KVSTORE_READ_ERROR             = -1,
    KVSTORE_WRITE_ERROR            = -2,
    KVSTORE_NOT_FOUND              = -3,
    KVSTORE_DATA_CORRUPT           = -4,
    KVSTORE_BAD_VALUE              = -5,
    KVSTORE_NO_SPACE_ON_DEVICE     = -6,
    KVSTORE_OS_ERROR               = -7,
    KVSTORE_WRITE_ONCE_ERROR       = -8,
    KVSTORE_AUTH_ERROR             = -9,
    KVSTORE_RBP_AUTH_ERROR         = -10,
    KVSTORE_MAX_KEYS_REACHED       = -11,
    KVSTORE_UNINITIALIZED          = -12,
} kvstore_status_e;



/** KVStore class
 *
 *  Interface class for Key Value Storage
 */

class KVStore {
public:
    enum create_flags {
        WRITE_ONCE_FLAG       = (1 << 0),
        ENCRYPT_FLAG          = (1 << 1),
        AUTHENTICATE_FLAG     = (1 << 2),
        ROLLBACK_PROTECT_FLAG = (1 << 3),
    };

    static const uint32_t MAX_KEY_SIZE = 128;

    typedef struct _opaque_set_handle *set_handle_t;

    typedef struct _opaque_key_iterator *iterator_t;

    typedef struct info {
        size_t size;
        uint32_t flags;
    } info_t;

    virtual ~KVStore() {};

    /**
     * @brief Initialize KVStore
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int init() = 0;

    /**
     * @brief Deinitialize KVStore
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int deinit() = 0;


    /**
     * @brief Reset KVStore contents (clear all keys)
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int reset() = 0;

    /**
     * @brief Set one KVStore item, given key and value.
     *
     * @param[in]  key                  Key.
     * @param[in]  buffer               Value data buffer.
     * @param[in]  size                 Value data size.
     * @param[in]  create_flags         Flag mask.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int set(const char *key, const void *buffer, size_t size, uint32_t create_flags);

    /**
     * @brief Get one KVStore item, given key.
     *
     * @param[in]  key                  Key.
     * @param[in]  buffer               Value data buffer.
     * @param[in]  buffer_size          Value data buffer size.
     * @param[out] actual_size          Actual read size.
     * @param[in]  offset               Offset to read from in data.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int get(const char *key, void *buffer, size_t buffer_size, size_t *actual_size = NULL, size_t offset = 0);

    /**
     * @brief Get information of a given key.
     *
     * @param[in]  key                  Key.
     * @param[out] info                 Returned information structure.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int get_info(const char *key, info_t *info);

    /**
     * @brief Remove a KVStore item, given key.
     *
     * @param[in]  key                  Key.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int remove(const char *key);


    /**
     * @brief Start an incremental KVStore set sequence.
     *
     * @param[out] handle               Returned incremental set handle.
     * @param[in]  key                  Key.
     * @param[in]  final_data_size      Final value data size.
     * @param[in]  create_flags         Flag mask.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int set_start(set_handle_t *handle, const char *key, size_t final_data_size, uint32_t create_flags);

    /**
     * @brief Add data to incremental KVStore set sequence.
     *
     * @param[in]  handle               Incremental set handle.
     * @param[in]  value_data           value data to add.
     * @param[in]  data_size            value data size.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int set_add_data(set_handle_t handle, const void *value_data, size_t data_size);

    /**
     * @brief Finalize an incremental KVStore set sequence.
     *
     * @param[in]  handle               Incremental set handle.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int set_finalize(set_handle_t handle);

    /**
     * @brief Start an iteration over KVStore keys.
     *
     * @param[out] it                   Returned iterator handle.
     * @param[in]  prefix               Key prefix (null for all keys).
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int iterator_open(iterator_t *it, const char *prefix = NULL);

    /**
     * @brief Get next key in iteration.
     *
     * @param[in]  it                   Iterator handle.
     * @param[in]  key                  Buffer for returned key.
     * @param[in]  key_size             Key buffer size.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int iterator_next(iterator_t it, char *key, size_t key_size);

    /**
     * @brief Close iteration.
     *
     * @param[in]  it                   Iterator handle.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int iterator_close(iterator_t it);
};
/** @}*/

#endif
