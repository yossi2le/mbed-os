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

//#if defined(MBEDTLS_ENTROPY_NV_SEED) && defined(NVSTORE_ENABLED)
EXTERNC int platform_std_nv_seed_read( unsigned char *buf, size_t buf_len )
{
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!platform_std_nv_seed_read entered\n");
    uint16_t in_size = buf_len;
    uint16_t out_size = 0;
    NVStore& nvstore = NVStore::get_instance();
    int nvStatus = nvstore.get(NVSTORE_DRBG_KEY, in_size, buf, out_size);

    if (NVSTORE_SUCCESS != nvStatus) {
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!platform_std_nv_seed_read error\n");
    }
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!platform_std_nv_seed_read success\n");

    return nvStatus;
}

EXTERNC int platform_std_nv_seed_write( unsigned char *buf, size_t buf_len )
{
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!platform_std_nv_seed_write entered\n");
    NVStore& nvstore = NVStore::get_instance();
    int nvStatus = nvstore.set(NVSTORE_DRBG_KEY, (uint16_t)buf_len, buf);

    if (NVSTORE_SUCCESS != nvStatus) {
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!platform_std_nv_seed_write error\n");
    }
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!platform_std_nv_seed_write success\n");

    return nvStatus;

}
//#endif /* MBEDTLS_ENTROPY_NV_SEED */

