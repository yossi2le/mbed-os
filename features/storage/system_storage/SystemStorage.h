#ifndef MBED_SYSTEM_STORAGE_H
#define MBED_SYSTEM_STORAGE_H

#include "stddef.h"
#include "stdint.h"
#include "BlockDevice.h"
#include "FileSystem.h"

#if COMPONENT_SPIF
#include "SPIFBlockDevice.h"
#include "LittleFileSystem.h"
#elif COMPONENT_SD
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#endif

namespace mbed {
namespace SystemStorage {

BlockDevice *get_default_blockdevice();

FileSystem *get_default_filesystem();

}
}
#endif
