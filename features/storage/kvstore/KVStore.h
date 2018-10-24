/* mbed Microcontroller Library
 * Copyright (c) 2018 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MBED_KEY_VALUE_STORE_H
#define MBED_KEY_VALUE_STORE_H

#include "FileSystem.h"

namespace mbed {

enum kv_create_flags {
    KV_WRITE_ONCE_FLAG       = (1 << 0),
    KV_ENCRYPT_FLAG          = (1 << 1),
    KV_AUTHENTICATE_FLAG     = (1 << 2),
    KV_ROLLBACK_PROTECT_FLAG = (1 << 3),
};

static const uint32_t KV_MAX_KEY_LENGTH = 128;
typedef struct _opaque_set_handle *kv_set_handle_t;
typedef struct _opaque_key_iterator *kv_iterator_t;

typedef struct info {
    size_t size;
    uint32_t flags;
} kv_info_t;

// Core API
int kv_set(const char *full_name_key, const void *buffer, size_t size, uint32_t create_flags);
int kv_get(const char *full_name_key, void *buffer, size_t buffer_size, size_t *actual_size);
int kv_get_info(const char *full_name_key, kv_info_t *info);
int kv_remove(const char *full_name_key);

// Key iterator
int kv_iterator_open(kv_iterator_t *it, const char *full_prefix = NULL);
int kv_iterator_next(kv_iterator_t it, char *key, size_t key_size);
int kv_iterator_close(kv_iterator_t it);

/** KVStore for Secure Store
 *
 *  @code
 *  ...
 *  @endcode
 */

class KVStore {

public:
    enum create_flags {
        WRITE_ONCE_FLAG       = (1 << 0),
        ENCRYPT_FLAG          = (1 << 1),
        AUTHENTICATE_FLAG     = (1 << 2),
        ROLLBACK_PROTECT_FLAG = (1 << 3),
    };

    static const uint32_t MAX_KEY_LENGTH = 128;

    typedef struct _opaque_set_handle *set_handle_t;

    typedef struct _opaque_key_iterator *iterator_t;

    typedef struct info {
        size_t size;
        uint32_t flags;
    } info_t;

    // Initialization and reset
    virtual int init();
    virtual int deinit();
    virtual int reset();

    // Core API
    virtual int set(const char *key, const void *buffer, size_t size, uint32_t create_flags);
    virtual int get(const char *key, void *buffer, size_t buffer_size, size_t *actual_size = NULL, size_t offset = 0);
    virtual int get_info(const char *key, info_t *info);
    virtual int remove(const char *key);
 
    // Incremental set API
    virtual int set_start(set_handle_t *handle, const char *key, size_t final_data_size, uint32_t create_flags);
    virtual int set_add_data(set_handle_t handle, const void *value_data, size_t data_size);
    virtual int set_finalize(set_handle_t handle);
 
    // Key iterator
    virtual int iterator_open(iterator_t *it, const char *prefix = NULL);
    virtual int iterator_next(iterator_t it, char *key, size_t key_size);
    virtual int iterator_close(iterator_t it);
};
////Important data structures
//// Key metadata
//typedef struct {
//    uint32_t magic;
//    uint16_t metadata_size;
//    uint16_t revision;
//    uint32_t user_flags;
//    uint32_t data_size;
//} key_metadata_t;
//
//// incremental set handle
//typedef struct {
//    char *key;
//    uint32_t create_flags;
//    size_t data_size;
//} inc_set_handle_t;
//
//// iterator handle
//typedef struct {
//    void *dir_handle;
//    char *prefix;
//} key_iterator_handle_t;

} //namespace mbed
#endif //MBED_KEY_VALUE_STORE_H
