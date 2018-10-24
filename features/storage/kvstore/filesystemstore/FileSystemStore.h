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
#ifndef MBED_FILE_SYSTEM_STORE_H
#define MBED_FILE_SYSTEM_STORE_H

#include "KVStore.h"
#include "FileSystem.h"

#define FSST_REVISION 1
#define FSST_MAGIC 0x46535354 // "FSST" hex 'magic' signature
#define FSST_PATH_NAME_SIZE 16
#define FSST_MAX_KEYS 256


namespace mbed {

/** Enum FSST standard status error codes
 *
 *  @enum FSST
 */
typedef enum {
    FSST_ERROR_OK               	 = 0,     /*!< no error */
    FSST_ERROR_NOT_FOUND		     = -3,
    FSST_ERROR_CORRUPTED_DATA		 = -4,
    FSST_ERROR_INVALID_INPUT		 = -5,
    FSST_ERROR_FS_OPERATION_FAILED   = -7,
    FSST_ERROR_WRITE_ONCE			 = -8,
    FSST_ERROR_MAX_KEYS_REACHED 	 = -11,
    FSST_ERROR_NOT_INITIALIZED  	 = -12
} FSST_status_e;


//Important data structures
// Key metadata
typedef struct {
    uint32_t magic;
    uint16_t metadata_size;
    uint16_t revision;
    uint32_t user_flags;
    uint32_t data_size;
} key_metadata_t;

// incremental set handle
typedef struct {
    char *key;
    uint32_t create_flags;
    size_t data_size;
} inc_set_handle_t;

// iterator handle
typedef struct {
    void *dir_handle;
    char *prefix;
} key_iterator_handle_t;


/** FileSystemStore for Secure Store
 *
 *  @code
 *  ...
 *  @endcode
 */

class FileSystemStore : KVStore {

public:
    FileSystemStore(/*size_t max_keys,*/ FileSystem *fs);
    virtual ~FileSystemStore() {}

    // Initialization and reset
    virtual int init();
    virtual int deinit();
    virtual int reset();

    // Core API
    virtual int set(const char *key, const void *buffer, size_t size, uint32_t create_flags);
    virtual int get(const char *key, void *buffer, size_t buffer_size, size_t *actual_size, size_t offset = 0);
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

private:
    int _build_full_path_key(const char *key_stc);
    int _verify_key_file(const char *key, key_metadata_t *key_metadata, File *kv_file);
    int _strip_full_path_from_key(char **stripped_key_ptr_dst, char *full_path_key_src);

private:
    FileSystem *_fs;
    PlatformMutex _mutex;

    size_t _max_keys;
    size_t _num_keys;
    bool _is_initialized;
    char _cfg_fs_path[FSST_PATH_NAME_SIZE + 1];
    char _full_path_key[FSST_PATH_NAME_SIZE + KVStore::MAX_KEY_SIZE + 1];
    size_t _cur_inc_data_size;
};


} //namespace mbed
#endif //MBED_FILE_SYSTEM_STORE_H
