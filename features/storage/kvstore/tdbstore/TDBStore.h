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

#ifndef MBED_TDBSTORE_H
#define MBED_TDBSTORE_H

#include <stdint.h>
#include <stdio.h>
#include "KVStore.h"
#include "BlockDevice.h"
#include "BufferedBlockDevice.h"
#include "PlatformMutex.h"

namespace mbed {

/** TDBStore class
 *
 *  Lightweight Key Value storage over a block device
 */

class TDBStore : public KVStore {
public:

    static const uint32_t RESERVED_AREA_SIZE = 64;

    /**
     * @brief Class constructor
     *
     * @param[in]  bd                   Underlying block device.
     * @param[in]  max_keys             Maximum number of keys (0 if unlimited).
     *
     * @returns none
     */
    TDBStore(BlockDevice *bd);

    /**
     * @brief Class destructor
     *
     * @returns none
     */
    virtual ~TDBStore();

    /**
     * @brief Initialize KVStore
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int init();

    /**
     * @brief Deinitialize KVStore
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int deinit();


    /**
     * @brief Reset KVStore contents (clear all keys)
     *        Warning: This function is not thread safe.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int reset();

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
    virtual int get(const char *key, void *buffer, size_t buffer_size, size_t *actual_size = NULL,
                    size_t offset = 0);

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

    /**
     * @brief Set data in reserved area.
     *
     * @param[in]  reserved_data        Reserved data buffer.
     * @param[in]  reserved_data_buf_size
     *                                  Reserved data buffer size.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int reserved_data_set(const void *reserved_data, size_t reserved_data_buf_size);

    /**
     * @brief Get data from reserved area.
     *
     * @param[in]  reserved_data        Reserved data buffer.
     * @param[in]  reserved_data_buf_size
     *                                  Reserved data buffer size.
     *
     * @returns 0 on success or a negative error code on failure
     */
    virtual int reserved_data_get(void *reserved_data, size_t reserved_data_buf_size);

private:

    typedef struct {
        uint32_t address;
        size_t   size;
    } tdbstore_area_data_t;

    static const int _num_areas = 2;

    PlatformMutex _mutex;
    void *_ram_table;
    size_t _max_keys;
    size_t _num_keys;
    BlockDevice *_bd;
    BufferedBlockDevice *_buff_bd;
    uint32_t _free_space_offset;
    uint32_t _master_record_offset;
    bool _is_initialized;
    int _active_area;
    uint16_t _active_area_version;
    size_t _size;
    tdbstore_area_data_t _area_params[_num_areas];
    uint32_t _prog_size;
    uint8_t *_work_buf;
    char *_key_buf;
    bool _variant_bd_erase_unit_size;
    void *_inc_set_handle;

    /**
     * @brief Read a block from an area.
     *
     * @param[in]  area                   Area.
     * @param[in]  offset                 Offset in area.
     * @param[in]  size                   Number of bytes to read.
     * @param[in]  buf                    Output buffer.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int read_area(uint8_t area, uint32_t offset, uint32_t size, void *buf);

    /**
     * @brief Write a block to an area.
     *
     * @param[in]  area                   Area.
     * @param[in]  offset                 Offset in area.
     * @param[in]  size                   Number of bytes to write.
     * @param[in]  buf                    Input buffer.
     *
     * @returns 0 for success, non-zero for failure.
     */
    int write_area(uint8_t area, uint32_t offset, uint32_t size, const void *buf);

    /**
     * @brief Reset an area (erase its start).
     *
     * @param[in]  area                   Area.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int reset_area(uint8_t area);

    /**
     * @brief Erase an erase unit.
     *
     * @param[in]  area                   Area.
     * @param[in]  offset                 Offset in area.
     *
     * @returns 0 for success, non-zero for failure.
     */
    int erase_erase_unit(uint8_t area, uint32_t offset);

    /**
     * @brief Calculate addresses and sizes of areas.
     */
    void calc_area_params();

    /**
     * @brief Read a TDBStore record from a given location.
     *
     * @param[in]  area                   Area.
     * @param[in]  offset                 Offset of record in area.
     * @param[in]  key                    Key.
     * @param[in]  data_buf               Data buffer.
     * @param[in]  data_buf_size          Data buffer size.
     * @param[out] actual_data_size       Actual data size.
     * @param[in]  data_offset            Offset in data.
     * @param[in]  copy_key               Copy key to user buffer.
     * @param[in]  copy_data              Copy data to user buffer.
     * @param[in]  check_expected_key     Check whether key belongs to this record.
     * @param[in]  calc_hash              Calculate hash (on key).
     * @param[out] hash                   Calculated hash.
     * @param[out] flags                  Record flags.
     * @param[out] next_offset            Offset of next record.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int read_record(uint8_t area, uint32_t offset, char *key,
                    void *data_buf, uint32_t data_buf_size,
                    uint32_t& actual_data_size, size_t data_offset, bool copy_key,
                    bool copy_data, bool check_expected_key, bool calc_hash,
                    uint32_t& hash, uint32_t& flags, uint32_t& next_offset);

    /**
     * @brief Write a master record of a given area.
     *
     * @param[in]  area                   Area.
     * @param[in]  version                Area version.
     * @param[out] next_offset            Offset of next record.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int write_master_record(uint8_t area, uint16_t version, uint32_t& next_offset);

    /**
     * @brief Copy a record from one area to the opposite one.
     *
     * @param[in]  from_area              Area to copy record from.
     * @param[in]  from_offset            Offset in source area.
     * @param[in]  to_offset              Offset in destination area.
     * @param[out] to_next_offset         Offset of next record in destination area.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int copy_record(uint8_t from_area, uint32_t from_offset, uint32_t to_offset,
                    uint32_t& to_next_offset);

    /**
     * @brief As part of GC process, copy all records in RAM table to the opposite area.
     *
     * @param[in]  from_area              Area to copy record from.
     * @param[in]  to_offset              Offset in destination area.
     * @param[out] to_next_offset         Offset of next record in destination area.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int copy_all_records(uint8_t from_area, uint32_t to_offset, uint32_t& to_next_offset);

    /**
     * @brief Garbage collection (compact all records from active area to the standby one).
     *
     * @returns 0 for success, nonzero for failure.
     */
    int garbage_collection();

    /**
     * @brief Return record size given key and data size.
     *
     * @param[in]  key                    Key.
     * @param[in]  data_size              Data size.
     *
     * @returns record size.
     */
    uint32_t record_size(const char *key, uint32_t data_size);

    /**
     * @brief Find a record given key
     *
     * @param[in]  area                   Area.
     * @param[in]  key                    Key.
     * @param[out] offset                 Offset of record.
     * @param[out] ram_table_ind          Index in ram table (target one if not found).
     * @param[out] hash                   Calculated key hash.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int find_record(uint8_t area, const char *key, uint32_t& offset,
                    uint32_t& ram_table_ind, uint32_t& hash);
    /**
     * @brief Actual logics of get API (covers also all other get APIs).
     *
     * @param[in]  key                  Key.
     * @param[in]  copy_data            Copy data to user buffer.
     * @param[in]  data_buf             Buffer to store data on.
     * @param[in]  data_buf_size        Data buffer size (bytes).
     * @param[out] actual_data_size     Actual data size (bytes).
     * @param[out] flags                Flags.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int do_get(const char *key, bool copy_data,
               void *data_buf, uint32_t data_buf_size, uint32_t& actual_data_size,
               uint32_t& flags);

    /**
     * @brief Actual logics of set API (covers also the remove API).
     *
     * @param[in]  key                  Key.
     * @param[in]  data_buf             Data buffer.
     * @param[in]  data_buf_size        Data buffer size (bytes).
     * @param[in]  flags                Flags.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int do_set(const char *key, const void *data_buf, uint32_t data_buf_size, uint32_t flags);

    /**
     * @brief Build RAM table and update _free_space_offset (scanning all the records in the area).
     *
     * @returns 0 for success, nonzero for failure.
     */
    int build_ram_table();

    /**
     * @brief Increment max number of keys and reallocate ram table accordingly.
     *
     * @param[out] ram_table             Updated ram table.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int increment_max_keys(void **ram_table = 0);

    /**
     * @brief Calculate offset from start of erase unit.
     *
     * @param[in]  area                  Area.
     * @param[in]  offset                Offset in area.
     * @param[out] offset_from_start     Offset from start of erase unit.
     * @param[out] dist_to_end           Distance to end of erase unit.
     *
     * @returns offset in erase unit.
     */
    void offset_in_erase_unit(uint8_t area, uint32_t offset, uint32_t& offset_from_start,
                              uint32_t& dist_to_end);

    /**
     * @brief Check whether erase unit is erased (from offset until end of unit).
     *
     * @param[in]  area                  Area.
     * @param[in]  offset                Offset in area.
     * @param[out] erased                Unit is erased.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int is_erase_unit_erased(uint8_t area, uint32_t offset, bool& erased);

    /**
     * @brief Before writing a record, check whether we are crossing an erase unit.
     *        If we do, check if it's erased, and erase it if not.
     *
     * @param[in]  area                  Area.
     * @param[in]  offset                Offset in area.
     * @param[in]  size                  Write size.
     *
     * @returns 0 for success, nonzero for failure.
     */
    int check_erase_before_write(uint8_t area, uint32_t offset, uint32_t size);

};
/** @}*/

} // namespace mbed

#endif
