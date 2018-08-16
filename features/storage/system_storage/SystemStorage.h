#ifndef MBED_SYSTEM_STORAGE_H
#define MBED_SYSTEM_STORAGE_H

#include "stddef.h"
#include "stdint.h"
#include "BlockDevice.h"
#include "FileSystem.h"

#if COMPONENT_SPIF
#include "SPIFBlockDevice.h"
#endif

#if COMPONENT_DATAFLASH
#include "DataFlashBlockDevice.h"
#endif

#if COMPONENT_SD
#include "SDBlockDevice.h"
#endif

#include "FATFileSystem.h"
#include "LittleFileSystem.h"

namespace mbed {

/** Gets the system default BlockDevice instance
    *  The default block device is dependent on components configuration in target.json.
    *  The supported block devices are:
    *       1. SPIFBlockDevice
    *       2. DataFlashBlockDevice
    *       3. SDBlockDevice
    *  The above list also define the order of importance in case more then
    *  one component is active.
    *
    *  @return    failure: NULL, no default block device has been defined
    *             success: a pointer to the system default block device
    */
BlockDevice *get_default_blockdevice();

/** Gets the system default filesystem instance
    *  The default filesystem is dependent on the default block device configured
    *  for the system. The supported filesystem are:
    *       1. LittleFileSystem - if SPIF or DATAFLASH are configured
    *       2. FATFileSystem - if SD is configured
    *  The default filesystem is mounted on "/default_fs".
    *
    *  @return    failure: NULL, no default filesystem has been defined
    *             success: a pointer to the system default filesystem
    */
FileSystem *get_default_filesystem();

}
#endif
