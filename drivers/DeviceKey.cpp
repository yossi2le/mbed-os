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

#include "drivers/DeviceKey.h"
#include "mbedtls/config.h"
#include "mbedtls/cmac.h"
#include <nvstore.h>
#include "trng_api.h"

namespace mbed {

DeviceKey::DeviceKey()
{
    return;
}

DeviceKey::~DeviceKey()
{
    return;
}

#if !defined(MBEDTLS_CMAC_C)
#error [NOT_SUPPORTED] MBEDTLS_CMAC_C needs to be enabled for this driver
#else

int DeviceKey::device_key_derived_key(const unsigned char *salt, size_t isalt_size, unsigned char *output, uint16_t ikey_type)
{
    uint32_t key_buff[DEVICE_KEY_32BYTE / sizeof(uint32_t)];
    size_t actual_size = DEVICE_KEY_32BYTE;

    if (DEVICE_KEY_16BYTE != ikey_type && DEVICE_KEY_32BYTE != ikey_type)
    {
        return DEVICEKEY_INVALID_KEY_TYPE;
    }

    //First try to read the key form NVStore
    int ret = read_key_from_nvstore(key_buff, actual_size);
    if (DEVICEKEY_SUCCESS != ret && DEVICEKEY_NOT_FOUND != ret)
    {
        return ret;
    }

    //If the key was not found in NVStore we will create it by using TRNG and then save it to NVStore
    if (DEVICEKEY_NOT_FOUND == ret) {
#if defined(DEVICE_TRNG)
        ret = generate_key_by_trng(key_buff,actual_size);
        if (DEVICEKEY_SUCCESS != ret) {
            return ret;
        }

        ret = device_inject_root_of_trust(key_buff,actual_size);
        if (DEVICEKEY_SUCCESS != ret) {
            return ret;
        }
#else
        return DEVICEKEY_NO_KEY_INJECTED;
#endif

    }

    ret = get_derive_key(key_buff, actual_size, salt, isalt_size, output, ikey_type);
    return ret;
}

int DeviceKey::device_inject_root_of_trust(uint32_t *value, size_t isize)
{
    size_t key_type;
    if (isize == 16) {
        key_type = DEVICE_KEY_16BYTE;
    } else if (isize == 32) {
        key_type = DEVICE_KEY_32BYTE;
    } else {
        return (DEVICEKEY_INVALID_KEY_SIZE);
    }

    return write_key_to_nvstore(value, key_type);
}

int DeviceKey::write_key_to_nvstore(uint32_t *value, size_t isize)
{
    if (isize > 32 || isize < 16) {
        return (DEVICEKEY_INVALID_KEY_SIZE);
    }

    NVStore &nvstore = NVStore::get_instance();
    int nvStatus = nvstore.set_once(NVSTORE_KEY_ROT, (uint16_t)isize, value);
    if (NVSTORE_ALREADY_EXISTS == nvStatus) {
        return DEVICEKEY_ALREADY_EXIST;
    }

    if (NVSTORE_WRITE_ERROR == nvStatus || NVSTORE_BUFF_TOO_SMALL == nvStatus) {
        return DEVICEKEY_SAVE_FAILED;
    }

    if (NVSTORE_SUCCESS != nvStatus) {
        return DEVICEKEY_NVSTORE_UNPREDICTABLE_ERROR;
    }

    return DEVICEKEY_SUCCESS;
}

int DeviceKey::read_key_from_nvstore(uint32_t *output, size_t &size)
{
    uint16_t short_size = size;
    NVStore &nvstore = NVStore::get_instance();
    int nvStatus = nvstore.get(NVSTORE_KEY_ROT, short_size, output, short_size);
    if (NVSTORE_NOT_FOUND == nvStatus) {
        return DEVICEKEY_NOT_FOUND;
    }

    if (NVSTORE_READ_ERROR == nvStatus || NVSTORE_BUFF_TOO_SMALL == nvStatus) {
        return DEVICEKEY_READ_FAILED;
    }

    if (NVSTORE_SUCCESS != nvStatus) {
        return DEVICEKEY_NVSTORE_UNPREDICTABLE_ERROR;
    }

    size = short_size;
    return DEVICEKEY_SUCCESS;
}

// Calculate CMAC functions - wrapper for mbedtls start/update and finish
int DeviceKey::calc_cmac(const unsigned char *input, size_t isize, uint32_t *ikey_buff, int ikey_size, unsigned char *output)
{
    int ret;
    mbedtls_cipher_context_t ctx;

    mbedtls_cipher_type_t mbedtls_cipher_type=MBEDTLS_CIPHER_AES_128_ECB;
    if (DEVICE_KEY_32BYTE == ikey_size) {
        mbedtls_cipher_type=MBEDTLS_CIPHER_AES_256_ECB;
    }

    const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(mbedtls_cipher_type);

    mbedtls_cipher_init(&ctx);
    if ((ret = mbedtls_cipher_setup(&ctx, cipher_info)) != 0) {
       goto finish;
    }

    ret = mbedtls_cipher_cmac_starts(&ctx, (unsigned char *)ikey_buff, ikey_size*8);
    if (ret != 0) {
       goto finish;
    }

    ret = mbedtls_cipher_cmac_update(&ctx, input, isize);
    if (ret != 0) {
       goto finish;
    }

    ret = mbedtls_cipher_cmac_finish(&ctx, output);
    if (ret != 0) {
       goto finish;
    }

    return DEVICEKEY_SUCCESS;

finish:
    mbedtls_cipher_free( &ctx );
    return ret;
}

int DeviceKey::get_derive_key(uint32_t *ikey_buff, size_t ikey_size, const unsigned char *isalt,
                              size_t isalt_size, unsigned char *output, uint32_t ikey_type)
{
    int ret;
    unsigned char * double_size_salt;

    if (ikey_size == ikey_type || ikey_size > ikey_type) {
        ret = this->calc_cmac(isalt, isalt_size, ikey_buff, ikey_size, output);
        if (DEVICEKEY_SUCCESS != ret) {
            goto finish;
        }
    }

    if (ikey_size < ikey_type) {
        ret = this->calc_cmac(isalt, isalt_size, ikey_buff, ikey_size, output);
        if (DEVICEKEY_SUCCESS != ret) {
            goto finish;
        }

        //Double the salt size
        double_size_salt = new unsigned char[isalt_size * 2];
        memcpy(double_size_salt, isalt, isalt_size);
        memcpy(double_size_salt + isalt_size, isalt, isalt_size);

        ret = this->calc_cmac(double_size_salt, isalt_size*2, ikey_buff, ikey_size, output+ikey_size);
        if (DEVICEKEY_SUCCESS != ret) {
            goto finish;
        }
    }

    return DEVICEKEY_SUCCESS;

finish:
    if (double_size_salt != NULL) {
        delete[] double_size_salt;
    }

    printf("Crypto cipher CMAC status %d", ret);
    return DEVICEKEY_ERR_CMAC_GENERIC_FAILURE;
}

//This method is generating hardcoded 16 byte key for now!!!
int DeviceKey::generate_key_by_trng(uint32_t *ikey_buff, size_t &size)
{
    if (size < DEVICE_KEY_16BYTE)
    {
        return DEVICEKEY_BUFFER_TO_SMALL;
    }

    memcpy((void*)ikey_buff,(const void*)"1234567812345678",16);
    size=16;

    return DEVICEKEY_SUCCESS;
}

#endif

} // namespace mbed

