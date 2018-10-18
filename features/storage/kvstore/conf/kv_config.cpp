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

#include "kv_config.h"
#include "KVStore.h"
#include "kv_map.h"
#include "BlockDevice.h"
#include "FileSystem.h"
#include "FileSystemStore.h"
#include "SlicingBlockDevice.h"
#include "FATFileSystem.h"
#include "LittleFileSystem.h"
#include "TDBStore.h"
#include "mbed_error.h"
#include "FlashIAP.h"
#include "FlashSimBlockDevice.h"
#include "mbed_trace.h"
#define TRACE_GROUP "KVCFG"

#if COMPONENT_FLASHIAP
#include "FlashIAPBlockDevice.h"
#endif

#if COMPONENT_QSPIF
#include "QSPIFBlockDevice.h"
#endif

#if COMPONENT_SPIF
#include "SPIFBlockDevice.h"
#endif

#if COMPONENT_DATAFLASH
#include "DataFlashBlockDevice.h"
#endif

#if COMPONENT_SD
#include "SDBlockDevice.h"
#endif


using namespace mbed;

typedef struct {
    KVStore *kvstroe_main_instance;
    KVStore *internal_store;
    KVStore *external_store;
    BlockDevice *internal_bd;
    BlockDevice *external_bd;
    FileSystem *external_fs;
} kvstore_config_t;

static SingletonPtr<PlatformMutex> mutex;
static bool is_kv_config_initialize = false;
static kvstore_config_t kvstore_config;

#define INTERNAL_BLOCKDEVICE_NAME FLASHIAP

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

#define _GET_FILESYSTEM_concat(dev, ...) _get_filesystem_##dev(__VA_ARGS__)
#define GET_FILESYSTEM(dev, ...) _GET_FILESYSTEM_concat(dev, __VA_ARGS__)

#define _GET_BLOCKDEVICE_concat(dev, ...) _get_blockdevice_##dev(__VA_ARGS__)
#define GET_BLOCKDEVICE(dev, ...) _GET_BLOCKDEVICE_concat(dev, __VA_ARGS__)

static inline uint32_t align_up(uint64_t val, uint64_t size)
{
    return (((val - 1) / size) + 1) * size;
}

static inline uint32_t align_down(uint64_t val, uint64_t size)
{
    return (((val) / size)) * size;
}

int _get_addresses(BlockDevice *bd, bd_addr_t start_address, bd_size_t size, bd_addr_t *out_start_addr,
                   bd_addr_t *out_end_addr )
{
    bd_addr_t aligned_end_address;
    bd_addr_t end_address;
    bd_addr_t aligned_start_address;

    aligned_start_address = align_down(start_address, bd->get_erase_size(start_address));
    if (aligned_start_address != start_address) {
        tr_error("KV Config: Start address is not aligned. Better use %02llx", aligned_start_address);
        return -1;
    }

    if (size == 0) {
        (*out_start_addr) = aligned_start_address;
        (*out_end_addr) = bd->size();
        return 0;
    }

    if (size != 0) {
        end_address = start_address + size;
        aligned_end_address = align_up(end_address, bd->get_erase_size(end_address));
        if (aligned_end_address != end_address) {
            tr_error("KV Config: End address is not aligned. Consider changing the size parameter.");
            return -1;
        }
    }

    if (aligned_end_address > bd->size()) {
        tr_error("KV Config: End address is out of boundaries");
        return -1;
    }

    (*out_start_addr) = aligned_start_address;
    (*out_end_addr) = aligned_end_address;
    return 0;
}

FileSystem *_get_filesystem_FAT(BlockDevice *bd, const char *mount)
{
    static FATFileSystem sdcard(mount, bd);
    return &sdcard;

}

FileSystem *_get_filesystem_LITTLE(BlockDevice *bd, const char *mount)
{
    static LittleFileSystem flash(mount, bd);
    return &flash;
}

FileSystemStore *_get_fileSystemStore(FileSystem *fs)
{
    static FileSystemStore fss(fs);
    return &fss;
}

FileSystem *_get_filesystem_default(BlockDevice *bd, const char *mount)
{
#if COMPONENT_QSPIF || COMPONENT_SPIF || COMPONENT_DATAFLASH
    return _get_filesystem_LITTLE( bd, mount);
#elif COMPONENT_SD
    return _get_filesystem_FAT(bd, mount);
#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_FLASHIAP(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_FLASHIAP

    bd_size_t bd_final_size;
    bd_addr_t flash_end_address;
    bd_addr_t flash_start_address;
    bd_addr_t flash_first_writable_sector_address;
    bd_addr_t aligned_start_address;
    bd_addr_t aligned_end_address;
    bd_addr_t end_address;
    FlashIAP flash;

    int ret = flash.init();
    if (ret != 0) {
        return NULL;
    }

    //Get flash parameters before starting
    flash_first_writable_sector_address = align_up(FLASHIAP_ROM_END, flash.get_sector_size(FLASHIAP_ROM_END));
    flash_start_address = flash.get_flash_start();
    flash_end_address = flash_start_address + flash.get_flash_size();;

    if (start_address != 0) {

        if (start_address < flash_first_writable_sector_address) {
            tr_error("KV Config: Internal block device start address overlapped ROM address ");
            flash.deinit();
            return NULL;
        }
        aligned_start_address = align_down(start_address, flash.get_sector_size(start_address));
        if (start_address != aligned_start_address) {
            tr_error("KV Config: Internal block device start address is not aligned. Better use %02llx", aligned_start_address);
            flash.deinit();
            return NULL;
        }

        if (size == 0) {
            //will use 2 sector only.
            bd_final_size = (flash_end_address - start_address);

            static FlashIAPBlockDevice bd(start_address, bd_final_size);
            flash.deinit();
            return &bd;
        }

        if (size != 0) {

            end_address = start_address + size;
            if ( end_address > flash_end_address) {
                tr_error("KV Config: Internal block device end address is out of boundaries");
                flash.deinit();
                return NULL;
            }

            aligned_end_address = align_up(end_address, flash.get_sector_size(end_address - 1));
            if (end_address != aligned_end_address) {
                tr_error("KV Config: Internal block device start address is not aligned. Consider changing the size parameter");
                flash.deinit();
                return NULL;
            }

            static FlashIAPBlockDevice bd(start_address, size);
            flash.deinit();
            return &bd;
        }
    }

    bool request_default = false;
    if (start_address == 0 && size == 0) {
        request_default = true;
        size = 1;
    }

    start_address = flash_end_address - size;
    aligned_start_address = align_down(start_address, flash.get_sector_size(start_address));
    //Skip this check if default parameters are set (0 for base address and 0 size).
    //We will calculate the address and size by ourselves
    if (start_address != aligned_start_address && !request_default) {
        tr_error("KV Config: Internal block device start address is not aligned. Consider changing the size parameter");
        flash.deinit();
        return NULL;
    }

    if (request_default) {
        //update start_address to double the size for TDBStore needs
        bd_final_size = (flash_end_address - aligned_start_address) * 2;
        start_address = (flash_end_address - bd_final_size);
        aligned_start_address = align_down(start_address, flash.get_sector_size(start_address));
    } else {
        bd_final_size = (flash_end_address - aligned_start_address);
    }

    flash.deinit();

    if (aligned_start_address < flash_first_writable_sector_address) {
        tr_error("KV Config: Internal block device start address overlapped ROM address ");
        return NULL;
    }
    static FlashIAPBlockDevice bd(aligned_start_address, bd_final_size);
    return &bd;

#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_SPIF(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_SPIF

    bd_addr_t aligned_end_address;
    bd_addr_t aligned_start_address;

    static SPIFBlockDevice bd(
        MBED_CONF_SPIF_DRIVER_SPI_MOSI,
        MBED_CONF_SPIF_DRIVER_SPI_MISO,
        MBED_CONF_SPIF_DRIVER_SPI_CLK,
        MBED_CONF_SPIF_DRIVER_SPI_CS,
        MBED_CONF_SPIF_DRIVER_SPI_FREQ
    );

    if (bd.init() != MBED_SUCCESS) {
        tr_error("KV Config: SPIFBlockDevice init fail");
        return NULL;
    }

    if (start_address == 0 && size == 0 ) {
        return &bd;
    }

    if (_get_addresses(&bd, start_address, size, &aligned_start_address, &aligned_end_address) != 0 ) {
        tr_error("KV Config: Fail to get addresses for SlicingBlockDevice.");
        return NULL;
    }

    static SlicingBlockDevice sbd(&bd, aligned_start_address, aligned_end_address);
    return &sbd;

#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_QSPIF(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_QSPIF

    bd_addr_t aligned_end_address;
    bd_addr_t aligned_start_address;

    static QSPIFBlockDevice bd(
        QSPI_FLASH1_IO0,
        QSPI_FLASH1_IO1,
        QSPI_FLASH1_IO2,
        QSPI_FLASH1_IO3,
        QSPI_FLASH1_SCK,
        QSPI_FLASH1_CSN,
        QSPIF_POLARITY_MODE_0,
        MBED_CONF_QSPIF_QSPI_FREQ
    );

    if (bd.init() != MBED_SUCCESS) {
        tr_error("KV Config: QSPIFBlockDevice init fail");
        return NULL;
    }

    if (start_address == 0 && size == 0 ) {
        return &bd;
    }

    if (_get_addresses(&bd, start_address, size, &aligned_start_address, &aligned_end_address) != 0 ) {
        tr_error("KV Config: Fail to get addresses for SlicingBlockDevice.");
        return NULL;
    }

    static SlicingBlockDevice sbd(&bd, aligned_start_address, aligned_end_address);
    return &sbd;

#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_DATAFLASH(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_DATAFLASH

    bd_addr_t aligned_end_address;
    bd_addr_t aligned_start_address;

    static DataFlashBlockDevice bd(
        MBED_CONF_DATAFLASH_SPI_MOSI,
        MBED_CONF_DATAFLASH_SPI_MISO,
        MBED_CONF_DATAFLASH_SPI_CLK,
        MBED_CONF_DATAFLASH_SPI_CS
    );

    if (bd.init() != MBED_SUCCESS) {
        tr_error("KV Config: DataFlashBlockDevice init fail");
        return NULL;
    }

    if (start_address == 0 && size == 0 ) {
        return &bd;
    }

    if (_get_addresses(&bd, start_address, size, &aligned_start_address, &aligned_end_address) != 0 ) {
        tr_error("KV Config: Fail to get addresses for SlicingBlockDevice.");
        return NULL;
    }

    static SlicingBlockDevice sbd(&bd, aligned_start_address, aligned_end_address);
    return &sbd;


#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_SD(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_SD

    bd_addr_t aligned_end_address;
    bd_addr_t aligned_start_address;

    static SDBlockDevice bd(
        MBED_CONF_SD_SPI_MOSI,
        MBED_CONF_SD_SPI_MISO,
        MBED_CONF_SD_SPI_CLK,
        MBED_CONF_SD_SPI_CS
    );

    if (bd.init() != MBED_SUCCESS) {
        tr_error("KV Config: SDBlockDevice init fail");
        return NULL;
    }

#if MBED_CONF_STORAGE_STORAGE_TYPE == TDB_EXTERNAL_NO_RBP
//In TDBStore we have a constraint of 4GByte
    if (start_address == 0 && size == 0  && bd.size() < (uint32_t)(-1)) {
        return &bd;
    }

    size = size != 0 ? size : align_down(bd.size(), bd.get_erase_size(bd.size()));

    if (_get_addresses(&bd, start_address, size, &aligned_start_address, &aligned_end_address) != 0 ) {
        tr_error("KV Config: Fail to get addresses for SlicingBlockDevice.");
        return NULL;
    }

    if (aligned_end_address - aligned_start_address != (uint32_t)(aligned_end_address - aligned_start_address)) {
        aligned_end_address = (uint32_t)(-1);//Support up to 4G only
    }

#else
    if (start_address == 0 && size == 0) {
        return &bd;
    }

    if (_get_addresses(&bd, start_address, size, &aligned_start_address, &aligned_end_address) != 0 ) {
        tr_error("KV Config: Fail to get addresses for SlicingBlockDevice.");
        return NULL;
    }

#endif

    aligned_end_address = align_down(aligned_end_address, bd.get_erase_size(aligned_end_address));
    static SlicingBlockDevice sbd(&bd, aligned_start_address, aligned_end_address);
    return &sbd;

#else
    return NULL;
#endif
}

BlockDevice *_get_blockdevice_default(bd_addr_t start_address, bd_size_t size)
{
#if COMPONENT_QSPIF
    return _get_blockdevice_QSPIF(start_address, size);
#elif COMPONENT_SPIF
    return _get_blockdevice_SPIF(start_address, size);
#elif COMPONENT_DATAFLASH
    return _get_blockdevice_DATAFLASH(start_address, size);
#elif COMPONENT_SD
    return _get_blockdevice_SD(start_address, size);
#else
    tr_error("KV Config: No default component define in target.json for this target.");
    return NULL;
#endif
}

int _storage_config_TDB_INTERNAL()
{
    bd_size_t internal_size = MBED_CONF_STORAGE_TDB_INTERNAL_INTERNAL_SIZE;
    bd_addr_t internal_start_address = MBED_CONF_STORAGE_TDB_INTERNAL_INTERNAL_BASE_ADDRESS;

    kvstore_config.internal_bd = GET_BLOCKDEVICE(INTERNAL_BLOCKDEVICE_NAME, internal_start_address, internal_size);
    if (kvstore_config.internal_bd == NULL) {
        tr_error("KV Config: Fail to get internal BlockDevice.");
        return MBED_ERROR_FAILED_OPERATION;
    }

    int ret = kvstore_config.internal_bd->init();
    if (ret != MBED_SUCCESS) {
        tr_error("KV Config: Fail to init internal BlockDevice.");
        return MBED_ERROR_FAILED_OPERATION;
    }

    static TDBStore tdb_internal(kvstore_config.internal_bd);
    kvstore_config.internal_store = &tdb_internal;

    ret = kvstore_config.internal_store->init();
    if (ret != MBED_SUCCESS) {
        tr_error("KV Config: Fail to init internal TDBStore.");
        return ret;
    }
    kvstore_config.kvstroe_main_instance =
        kvstore_config.internal_store; //TODO: change this when secure storage come to live

    ret = kv_init();
    if (MBED_SUCCESS != ret) {
        tr_error("KV Config: Fail to init KVStore global API.");
        return ret;
    }

    ret = kv_attach(STR(MBED_CONF_STORAGE_DEFAULT_KV), kvstore_config.kvstroe_main_instance);
    if (MBED_SUCCESS != ret) {
        tr_error("KV Config: Fail to attach KVStore main instance to KVStore global API.");
        return ret;
    }

    return MBED_SUCCESS;
}

int _storage_config_TDB_EXTERNAL()
{
    bd_size_t internal_rbp_size = MBED_CONF_STORAGE_TDB_EXTERNAL_RBP_INTERNAL_SIZE;
    size_t rbp_num_of_enteries = MBED_CONF_STORAGE_TDB_EXTERNAL_RBP_NUMBER_OF_ENTRIES;
    bd_addr_t internal_start_address = MBED_CONF_STORAGE_TDB_EXTERNAL_INTERNAL_BASE_ADDRESS;

    if (internal_rbp_size == 0) {
        internal_rbp_size = 4 * 1024 * rbp_num_of_enteries / 32;
    }

    kvstore_config.internal_bd = GET_BLOCKDEVICE(INTERNAL_BLOCKDEVICE_NAME, internal_start_address, internal_rbp_size);
    if (kvstore_config.internal_bd == NULL) {
        tr_error("KV Config: Fail to get internal BlockDevice.");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    int ret = kvstore_config.internal_bd->init();
    if (ret != MBED_SUCCESS) {
        tr_error("KV Config: Fail to init internal BlockDevice.");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    static TDBStore tdb_internal(kvstore_config.internal_bd);
    kvstore_config.internal_store = &tdb_internal;

    ret = kvstore_config.internal_store->init();
    if (ret != MBED_SUCCESS) {
        tr_error("KV Config: Fail to init internal TDBStore.");
        return ret;
    }

    return _storage_config_TDB_EXTERNAL_NO_RBP();
}

int _storage_config_TDB_EXTERNAL_NO_RBP()
{
    bd_size_t size = MBED_CONF_STORAGE_TDB_EXTERNAL_NO_RBP_EXTERNAL_SIZE;
    bd_addr_t address = MBED_CONF_STORAGE_TDB_EXTERNAL_NO_RBP_EXTERNAL_BASE_ADDRESS;

    BlockDevice *bd = GET_BLOCKDEVICE(MBED_CONF_STORAGE_TDB_EXTERNAL_NO_RBP_BLOCKDEVICE, address, size);
    if (bd == NULL) {
        tr_error("KV Config: Fail to get external BlockDevice.");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    if ( strcmp(STR(MBED_CONF_STORAGE_TDB_EXTERNAL_NO_RBP_BLOCKDEVICE), "SD") == 0 ) {
        //TDBStore need FlashSimBlockDevice when working with SD block device
        if (bd->init() != MBED_SUCCESS) {
            tr_error("KV Config: Fail to init external BlockDevice.");
            return MBED_ERROR_FAILED_OPERATION ;
        }

        static FlashSimBlockDevice flash_bd(bd);
        kvstore_config.external_bd = &flash_bd;
    } else {
        kvstore_config.external_bd = bd;
    }

    int ret = kvstore_config.external_bd->init();
    if (ret != MBED_SUCCESS) {
        tr_error("KV Config: Fail to init external BlockDevice.");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    static TDBStore tdb_external(kvstore_config.external_bd);
    kvstore_config.external_store = &tdb_external;

    kvstore_config.kvstroe_main_instance =
        kvstore_config.external_store; //TODO: change this when secure storage come to live

    ret = kvstore_config.external_store->init();
    if (ret != MBED_SUCCESS) {
        tr_error("KV Config: Fail to init external TDBStore");
        return ret;
    }

    ret = kv_init();
    if (MBED_SUCCESS != ret) {
        tr_error("KV Config: Fail to init KVStore global API");
        return ret;
    }

    ret = kv_attach(STR(MBED_CONF_STORAGE_DEFAULT_KV), kvstore_config.kvstroe_main_instance);
    if (MBED_SUCCESS != ret) {
        tr_error("KV Config: Fail to attach KvStore main instance to KVStore global API");
        return ret;
    }

    return MBED_SUCCESS;
}

int _storage_config_FILESYSTEM()
{
    bd_size_t internal_rbp_size = MBED_CONF_STORAGE_FILESYSTEM_RBP_INTERNAL_SIZE;
    size_t rbp_num_of_enteries = MBED_CONF_STORAGE_FILESYSTEM_RBP_NUMBER_OF_ENTRIES;
    bd_addr_t internal_start_address = MBED_CONF_STORAGE_FILESYSTEM_INTERNAL_BASE_ADDRESS;

    if (internal_rbp_size == 0) {
        internal_rbp_size = 4 * 1024 * rbp_num_of_enteries / 32;
    }

    kvstore_config.internal_bd = GET_BLOCKDEVICE(INTERNAL_BLOCKDEVICE_NAME, internal_start_address, internal_rbp_size);
    if (kvstore_config.internal_bd == NULL) {
        tr_error("KV Config: Fail to get internal BlockDevice ");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    int ret = kvstore_config.internal_bd->init();
    if (ret != MBED_SUCCESS) {
        tr_error("KV Config: Fail to init internal BlockDevice ");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    static TDBStore tdb_internal(kvstore_config.internal_bd);
    kvstore_config.internal_store = &tdb_internal;

    ret = kvstore_config.internal_store->init();
    if (ret != MBED_SUCCESS) {
        tr_error("KV Config: Fail to init internal TDBStore");
        return ret;
    }

    return _storage_config_FILESYSTEM_NO_RBP();
}

int _storage_config_FILESYSTEM_NO_RBP()
{
    bd_size_t size = MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_EXTERNAL_SIZE;
    bd_addr_t address = MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_EXTERNAL_BASE_ADDRESS;
    const char *mount_point = STR(MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_MOUNT_POINT);

    kvstore_config.external_bd = GET_BLOCKDEVICE(MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_BLOCKDEVICE, address, size);
    if (kvstore_config.external_bd == NULL) {
        tr_error("KV Config: Fail to get external BlockDevice ");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    int ret = kvstore_config.external_bd->init();
    if (MBED_SUCCESS != ret) {
        tr_error("KV Config: Fail to init external BlockDevice ");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    kvstore_config.external_fs = GET_FILESYSTEM(MBED_CONF_STORAGE_FILESYSTEM_NO_RBP_FILESYSTEM, kvstore_config.external_bd,
                                 mount_point);
    if (kvstore_config.external_fs == NULL) {
        tr_error("KV Config: Fail to get FileSystem");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    ret = kvstore_config.external_fs->mount(kvstore_config.external_bd);
    if (ret != MBED_SUCCESS) {
        ret = kvstore_config.external_fs->reformat(kvstore_config.external_bd);
        if (ret != MBED_SUCCESS) {
            tr_error("KV Config: Fail to mount FileSystem to %s", mount_point);
            return MBED_ERROR_FAILED_OPERATION ;
        }
    }

    kvstore_config.external_store = _get_fileSystemStore(kvstore_config.external_fs);
    if (kvstore_config.external_store == NULL) {
        tr_error("KV Config: Fail to get FileSystemStore");
        return MBED_ERROR_FAILED_OPERATION ;
    }

    ret = kvstore_config.external_store->init();
    if (ret != MBED_SUCCESS) {
        tr_error("KV Config: Fail to init FileSystemStore");
        return ret;
    }

    kvstore_config.kvstroe_main_instance =
        kvstore_config.external_store; //TODO: change this when secure storage come to live

    ret = kv_init();
    if (MBED_SUCCESS != ret) {
        tr_error("KV Config: Fail to init KVStore global API");
        return ret;
    }

    ret = kv_attach(STR(MBED_CONF_STORAGE_DEFAULT_KV), kvstore_config.kvstroe_main_instance);
    if (MBED_SUCCESS != ret) {
        tr_error("KV Config: Fail to attach KvStore main instance to KVStore global API");
        return ret;
    }

    return MBED_SUCCESS;
}

MBED_WEAK int storage_configuration()
{

    int ret = MBED_SUCCESS;

    mutex->lock();

    if (is_kv_config_initialize) {
        goto exit;
    }

    memset(&kvstore_config, 0, sizeof(kvstore_config_t));

    ret = _STORAGE_CONFIG(MBED_CONF_STORAGE_STORAGE_TYPE);

    if (ret == MBED_SUCCESS) {
        is_kv_config_initialize = true;
    }

exit:
    mutex->unlock();
    return ret;
}
