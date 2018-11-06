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
#ifndef _KVSTORE_STATIC_API
#define _KVSTORE_STATIC_API

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _opaque_kv_key_iterator *kv_iterator_t;

#define KV_WRITE_ONCE_FLAG        (1 << 0)
#define KV_ENCRYPT_FLAG           (1 << 1)
#define KV_AUTHENTICATE_FLAG      (1 << 2)
#define KV_ROLLBACK_PROTECT_FLAG  (1 << 3)

static const uint32_t KV_MAX_KEY_LENGTH = 128;

typedef struct info {
    size_t size;
    uint32_t flags;
} kv_info_t;

/**
 * @brief Set one KVStore item, given key and value.
 *
 * @param[in]  full_name_key        /Partition_path/Key.
 * @param[in]  buffer               Value data buffer.
 * @param[in]  size                 Value data size.
 * @param[in]  create_flags         Flag mask.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_set(const char *full_name_key, const void *buffer, size_t size, uint32_t create_flags);

/**
 * @brief Get one KVStore item, given key.
 *
 * @param[in]  full_name_key        /Partition_path/Key.
 * @param[in]  buffer               Value data buffer.
 * @param[in]  buffer_size          Value data buffer size.
 * @param[out] actual_size          Actual read size.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_get(const char *full_name_key, void *buffer, size_t buffer_size, size_t *actual_size);

/**
 * @brief Get information of a given key.
 *
 * @param[in]  full_name_key        /Partition_path/Key.
 * @param[out] info                 Returned information structure.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_get_info(const char *full_name_key, kv_info_t *info);

/**
 * @brief Remove a KVStore item, given key.
 *
 * @param[in]  full_name_key        Partition_path/Key.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_remove(const char *full_name_key);

/**
 * @brief Start an iteration over KVStore keys to find all the entries
 *        that fit the full_prefix
 *
 * @param[out] it                   Allocating iterator handle.
 *                                  Do not forget to call kv_iterator_close
 *                                  to deallocate the memory.
 * @param[in]  full_prefix          full_prefix Partition/Key prefix. If
 *                                  empty key or NULL pointer, all keys
 *                                  will match.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_iterator_open(kv_iterator_t *it, const char *full_prefix = NULL);

/**
 * @brief Get next key in iteration that matches the prefix.
 *
 * @param[in]  it                   Iterator handle.
 * @param[in]  key                  Buffer for returned key.
 * @param[in]  key_size             Key buffer size.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_iterator_next(kv_iterator_t it, char *key, size_t key_size);

/**
 * @brief Close iteration and de-allocate the iterator handle.
 *
 * @param[in]  it                   Iterator handle.
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_iterator_close(kv_iterator_t it);

/**
 * @brief Remove all keys and related data
 *
 * @param[in]  kvstore_path        /Pratition/
 *
 * @returns 0 on success or a negative error code on failure
 */
int kv_reset(const char *kvstore_path);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif
#endif
