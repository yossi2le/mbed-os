#include "SystemStorage.h"

namespace mbed {
namespace SystemStorage{

MBED_WEAK BlockDevice* get_default_blockdevice()
{
#if COMPONENT_SPIF

    static SPIFBlockDevice default_bd(
        MBED_CONF_SPIF_DRIVER_SPI_MOSI, 
        MBED_CONF_SPIF_DRIVER_SPI_MISO,
        MBED_CONF_SPIF_DRIVER_SPI_CLK, 
        MBED_CONF_SPIF_DRIVER_SPI_CS, 
        MBED_CONF_SPIF_DRIVER_SPI_FREQ
        );

    return &default_bd;

#elif COMPONENT_SD

    static SDBlockDevice default_bd(
        MBED_CONF_SD_SPI_MOSI, 
        MBED_CONF_SD_SPI_MISO,
        MBED_CONF_SD_SPI_CLK, 
        MBED_CONF_SD_SPI_CS
        );

    return &default_bd;

#else

    return NULL;

#endif

}

MBED_WEAK FileSystem * get_default_filesystem()
{
#if COMPONENT_SPIF

    static LittleFileSystem default_fs("defult_fs", get_default_blockdevice());

    return &default_fs;

#elif COMPONENT_SD

    static FATFileSystem default_fs("defult_fs", get_default_blockdevice());

    return &default_fs;

#else

    return NULL;

#endif

}

}
}
