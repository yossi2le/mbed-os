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

#ifndef FSST_FOLDER_PATH
#define FSST_FOLDER_PATH "$fsst$" //default FileSystemStore folder path on fs
#endif

namespace mbed {

/** FileSystemStore for Secure Store
 *
 *  @code
 *  ...
 *  @endcode
 */
class FileSystemStore : public KVStore {

public:
    /** Create FileSystemStore - A Key Value API on top of FS
     *
     *  @param fs File system on top which File System Store is adding KV API
     */
    FileSystemStore(FileSystem *fs);

    /** Destroy FileSystemStore instance
     *
     */
    virtual ~FileSystemStore() {}

    /**
      * @brief Initialize FileSystemStore
      *
      * @returns 0 on success or a negative error code on failure
      */
    virtual int init();

    /**
      * @brief Deinitialize FileSystemStore
      *
      * @returns 0 on success or a negative error code on failure
      */
    virtual int deinit();

    /**
     * @brief Reset FileSystemStore contents (clear all keys)
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int reset();

    /**
     * @brief Set one FileSystemStore item, given key and value.
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
      * @brief Get one FileSystemStore item, given key.
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
     * @brief Remove a FileSystemStore item, given key.
     *
     * @param[in]  key                  Key.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int remove(const char *key);

    /**
     * @brief Start an incremental FileSystemStore set sequence.
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
     * @brief Add data to incremental FileSystemStore set sequence.
     *
     * @param[in]  handle               Incremental set handle.
     * @param[in]  value_data           value data to add.
     * @param[in]  data_size            value data size.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int set_add_data(set_handle_t handle, const void *value_data, size_t data_size);

    /**
     * @brief Finalize an incremental FileSystemStore set sequence.
     *
     * @param[in]  handle               Incremental set handle.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int set_finalize(set_handle_t handle);

    /**
     * @brief Start an iteration over FileSystemStore keys.
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

private:

    // Key metadata
    typedef struct {
        uint32_t magic;
        uint16_t metadata_size;
        uint16_t revision;
        uint32_t user_flags;
    } key_metadata_t;

    /**
     * @brief Build Full name class member from Key, as a combination of FSST folder and key name
     *
     * @param[in]  key_src              key file name
     *
     * @returns 0 on success or a negative error code on failure
     */
    int _build_full_path_key(const char *key_src);

    /**
     * @brief Verify Key file metadata validity and open it if valid
     *
     * @param[in]  key                  in validated key file name.
     * @param[in]  key_metadata         Returned key file metadata.
     * @param[in]  kv_file              opened kv file handle (unless file doesnt exist)
     *
     * @returns 0 on success or a negative error code on failure
     */
    int _verify_key_file(const char *key, key_metadata_t *key_metadata, File *kv_file);

private:
    FileSystem *_fs;
    PlatformMutex _mutex;
    PlatformMutex _inc_data_add_mutex;

    bool _is_initialized;
    char *_cfg_fs_path; /* FileSystemStore path name on FileSystem */
    size_t _cfg_fs_path_size; /* Size of configured FileSystemStore path name on FileSystem */
    char *_full_path_key; /* Full name of Key file currently working on */
    size_t _cur_inc_data_size; /* Amount of data added to Key file so far, during incremental add data */
    set_handle_t _cur_inc_set_handle; /* handle of currently key file under incremental set process */
};


} //namespace mbed
#endif //MBED_FILE_SYSTEM_STORE_H
