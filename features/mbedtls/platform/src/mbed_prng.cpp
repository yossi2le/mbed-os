/* mbed Microcontroller Library
 * Copyright (c) 2016 ARM Limited
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

//#include "mbed.h"
#include "platform_mbed.h"
#include "nvstore.h"

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#define NVSTORE_DRBG_KEY 5
#define TEMP_SEED_SIZE 64

//#if defined(MBEDTLS_ENTROPY_NV_SEED) && defined(NVSTORE_ENABLED)


EXTERNC int platform_inject_nv_seed( unsigned char *buf, size_t buf_len )
{
    //checkout there is no seed
    unsigned char temp_buf[TEMP_SEED_SIZE];
    uint16_t out_size = 0;

    NVStore& nvstore = NVStore::get_instance();
    int ret = nvstore.get(NVSTORE_DRBG_KEY, TEMP_SEED_SIZE, temp_buf, out_size);
    if (ret == 0 || ret == NVSTORE_BUFF_TOO_SMALL) {
        return NVSTORE_ALREADY_EXISTS;
    }

    if (ret != NVSTORE_NOT_FOUND) {
        return ret;
    }

    return platform_std_nv_seed_write(buf, buf_len);

}

EXTERNC int platform_std_nv_seed_read( unsigned char *buf, size_t buf_len )
{
    uint16_t in_size = buf_len;
    uint16_t out_size = 0;
    NVStore& nvstore = NVStore::get_instance();
    return nvstore.get(NVSTORE_DRBG_KEY, in_size, buf, out_size);
}

EXTERNC int platform_std_nv_seed_write( unsigned char *buf, size_t buf_len )
{
    NVStore& nvstore = NVStore::get_instance();
    return nvstore.set(NVSTORE_DRBG_KEY, (uint16_t)buf_len, buf);
}
//#endif /* MBEDTLS_ENTROPY_NV_SEED */

