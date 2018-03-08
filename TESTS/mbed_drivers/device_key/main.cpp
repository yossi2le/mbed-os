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

#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"
#include "DeviceKey.h"
#include "nvstore.h"

using namespace utest::v1;

/*
 * Test that wrong size of key is rejected when trying to persist a key
 */
void device_key_set_value_wrong_size_test()
{
    DeviceKey &devkey = DeviceKey::get_instance();
    uint32_t key[DEVICE_KEY_32BYTE / sizeof(uint32_t)];

    memcpy(key, "12345678123456788765432187654321", DEVICE_KEY_32BYTE);

    for (int i = 0; i < 50; i++) {
        if (DEVICE_KEY_16BYTE == i || DEVICE_KEY_32BYTE == i) {
            continue;
        }
        int ret = devkey.device_key_set_value(key, i);
        TEST_ASSERT_EQUAL_INT(DEVICEKEY_INVALID_KEY_SIZE, ret);
    }
}

/*
 * Test that a 16 byte size key is written to persistent memory
 */
void device_key_set_value_16_byte_size_test()
{
    DeviceKey &devkey = DeviceKey::get_instance();
    uint32_t rkey[DEVICE_KEY_32BYTE / sizeof(uint32_t)];
    uint16_t actual_size;
    uint32_t key[DEVICE_KEY_32BYTE / sizeof(uint32_t)];
    NVStore &nvstore = NVStore::get_instance();

    nvstore.init();
    int ret = nvstore.reset();
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    memcpy(key, "12345678123456788765432187654321", DEVICE_KEY_32BYTE);
    ret = devkey.device_key_set_value(key, DEVICE_KEY_16BYTE);
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    //Read the key from NVStore.
    memset(rkey, 0, DEVICE_KEY_32BYTE);
    ret = nvstore.get(NVSTORE_KEY_ROT, DEVICE_KEY_16BYTE, rkey, actual_size);
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);
    TEST_ASSERT_EQUAL_INT(DEVICE_KEY_16BYTE, actual_size);
    TEST_ASSERT_EQUAL_INT32_ARRAY(key, rkey, actual_size / sizeof(uint32_t));
}

/*
 * Test that a 32 byte size key is written to persistent memory
 */
void device_key_set_value_32_byte_size_test()
{
    DeviceKey &devkey = DeviceKey::get_instance();
    uint32_t rkey[DEVICE_KEY_32BYTE / sizeof(uint32_t)];
    uint16_t actual_size;
    uint32_t key[DEVICE_KEY_32BYTE / sizeof(uint32_t)];
    NVStore &nvstore = NVStore::get_instance();

    nvstore.init();
    nvstore.reset();
    int ret = devkey.device_key_set_value(key, DEVICE_KEY_32BYTE);
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    //Read the key from NVStore.
    memset(rkey, 0, DEVICE_KEY_32BYTE);
    ret = nvstore.get(NVSTORE_KEY_ROT, DEVICE_KEY_32BYTE, rkey, actual_size);
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);
    TEST_ASSERT_EQUAL_INT(DEVICE_KEY_32BYTE, actual_size);
    TEST_ASSERT_EQUAL_INT32_ARRAY(key, rkey, actual_size / sizeof(uint32_t));

}

/*
 * Test that a key can be written to persistent memory only once.
 */
void device_key_set_value_only_once_test()
{
    DeviceKey &devkey = DeviceKey::get_instance();
    uint32_t key[DEVICE_KEY_32BYTE / sizeof(uint32_t)];
    NVStore &nvstore = NVStore::get_instance();

    nvstore.init();
    int ret = nvstore.reset();
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    memcpy(key, "12345678123456788765432187654321", DEVICE_KEY_32BYTE);
    ret = devkey.device_key_set_value(key, DEVICE_KEY_16BYTE);
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    ret = devkey.device_key_set_value(key, DEVICE_KEY_16BYTE);
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_ALREADY_EXIST, ret);
}

/*
 * Test the consistency of derived 16 byte key result.
 */
void device_key_derive_key_consistency_16_byte_key_test()
{
    unsigned char output1[DEVICE_KEY_16BYTE];
    unsigned char output2[DEVICE_KEY_16BYTE];
    unsigned char empty_buffer[DEVICE_KEY_16BYTE];
    unsigned char salt[] = { "Once upon a time, I worked for the circus and I lived in Omaha." };
    int key_type = DEVICE_KEY_16BYTE;
    DeviceKey &devkey = DeviceKey::get_instance();
    NVStore &nvstore = NVStore::get_instance();

    nvstore.init();
    int ret = nvstore.reset();
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    size_t salt_size = sizeof(salt);
    memset(output1, 0, DEVICE_KEY_16BYTE);
    ret = devkey.device_key_derive_key(salt, salt_size, output1, key_type);
    TEST_ASSERT_EQUAL_INT32(0, ret);
    bool is_empty = !memcmp(empty_buffer, output1, sizeof(output1));
    TEST_ASSERT_FALSE(is_empty);

    for (int i = 0; i < 100; i++) {
        memset(output2, 0, DEVICE_KEY_16BYTE);
        ret = devkey.device_key_derive_key(salt, salt_size, output2, key_type);
        TEST_ASSERT_EQUAL_INT32(0, ret);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(output1, output2, DEVICE_KEY_16BYTE);
    }
}

/*
 * Test the consistency of derived 32 byte key result.
 */
void device_key_derive_key_consistency_32_byte_key_test()
{
    unsigned char output1[DEVICE_KEY_32BYTE];
    unsigned char output2[DEVICE_KEY_32BYTE];
    unsigned char empty_buffer[DEVICE_KEY_32BYTE];
    unsigned char salt[] = { "The quick brown fox jumps over the lazy dog" };
    int key_type = DEVICE_KEY_32BYTE;
    DeviceKey &devkey = DeviceKey::get_instance();
    NVStore &nvstore = NVStore::get_instance();

    nvstore.init();
    int ret = nvstore.reset();
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    size_t salt_size = sizeof(salt);
    memset(output1, 0, DEVICE_KEY_32BYTE);
    ret = devkey.device_key_derive_key(salt, salt_size, output1, key_type);
    TEST_ASSERT_EQUAL_INT32(0, ret);
    bool is_empty = !memcmp(empty_buffer, output1, sizeof(output1));
    TEST_ASSERT_FALSE(is_empty);

    for (int i = 0; i < 100; i++) {
        memset(output2, 0, DEVICE_KEY_32BYTE);
        ret = devkey.device_key_derive_key(salt, salt_size, output2, key_type);
        TEST_ASSERT_EQUAL_INT32(0, ret);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(output1, output2, DEVICE_KEY_32BYTE);
    }
}

/*
 * Test request for 16 byte key is returning a correct key size.
 */
void device_key_derive_key_key_type_16_test()
{
    unsigned char output[DEVICE_KEY_16BYTE * 2];
    unsigned char salt[] = "The quick brown fox jumps over the lazy dog";
    unsigned char expectedString[] = "Some String";
    int key_type = DEVICE_KEY_16BYTE;
    size_t salt_size = sizeof(salt);
    DeviceKey &devkey = DeviceKey::get_instance();
    NVStore &nvstore = NVStore::get_instance();

    nvstore.init();
    int ret = nvstore.reset();
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    memset(output, 0, DEVICE_KEY_16BYTE * 2);
    memcpy(output + DEVICE_KEY_16BYTE - sizeof(expectedString), expectedString, sizeof(expectedString));
    memcpy(output + DEVICE_KEY_16BYTE + 1, expectedString, sizeof(expectedString));

    ret = devkey.device_key_derive_key(salt, salt_size, output, key_type);
    TEST_ASSERT_EQUAL_INT32(0, ret);
    //Test that we didn't override the buffer after the 16 byte size
    TEST_ASSERT_EQUAL_UINT8_ARRAY(output + DEVICE_KEY_16BYTE + 1 , expectedString, sizeof(expectedString));
    //Test that we did override the buffer all 16 byte
    TEST_ASSERT(memcmp(output + DEVICE_KEY_16BYTE - sizeof(expectedString), expectedString, sizeof(expectedString)) != 0);
}

/*
 * Test request for 32 byte key is returning a correct key size.
 */
void device_key_derive_key_key_type_32_test()
{
    unsigned char output[DEVICE_KEY_32BYTE * 2];
    unsigned char salt[] = "The quick brown fox jumps over the lazy dog";
    int key_type = DEVICE_KEY_32BYTE;
    size_t salt_size = sizeof(salt);
    unsigned char expectedString[] = "Some String";
    DeviceKey &devkey = DeviceKey::get_instance();
    NVStore &nvstore = NVStore::get_instance();

    nvstore.init();
    int ret = nvstore.reset();
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    memset(output, 0, DEVICE_KEY_32BYTE * 2);
    memcpy(output + DEVICE_KEY_32BYTE - sizeof(expectedString), expectedString, sizeof(expectedString));
    memcpy(output + DEVICE_KEY_32BYTE + 1, expectedString, sizeof(expectedString));

    ret = devkey.device_key_derive_key(salt, salt_size, output, key_type);
    TEST_ASSERT_EQUAL_INT32(0, ret);
    //Test that we didn't override the buffer after the 32 byte size
    TEST_ASSERT_EQUAL_UINT8_ARRAY(output + DEVICE_KEY_32BYTE + 1, expectedString, sizeof(expectedString));
    //Test that we did override the buffer all 32 byte
    TEST_ASSERT(memcmp(output + DEVICE_KEY_32BYTE - sizeof(expectedString), expectedString, sizeof(expectedString)) != 0);
}

/*
 * Test request for unknown key size returns an error
 */
void device_key_derive_key_wrong_key_type_test()
{
    unsigned char output[DEVICE_KEY_16BYTE];
    unsigned char salt[] = "The quick brown fox jumps over the lazy dog";
    size_t salt_size = sizeof(salt);
    DeviceKey &devkey = DeviceKey::get_instance();
    NVStore &nvstore = NVStore::get_instance();

    nvstore.init();
    int ret = nvstore.reset();
    TEST_ASSERT_EQUAL_INT(DEVICEKEY_SUCCESS, ret);

    memset(output, 0, DEVICE_KEY_32BYTE);
    ret = devkey.device_key_derive_key(salt, salt_size, output, 12);//96 bit key type is not supported
    TEST_ASSERT_EQUAL_INT32(DEVICEKEY_INVALID_KEY_TYPE, ret);

}

utest::v1::status_t greentea_failure_handler(const Case *const source, const failure_t reason) {
    greentea_case_failure_abort_handler(source, reason);
    return STATUS_CONTINUE;
}

Case cases[] = { Case("Device Key - set value wrong size"              , device_key_set_value_wrong_size_test              , greentea_failure_handler),
                 Case("Device Key - set value 16 byte size"            , device_key_set_value_16_byte_size_test            , greentea_failure_handler),
                 Case("Device Key - set value 32 byte size"            , device_key_set_value_32_byte_size_test            , greentea_failure_handler),
                 Case("Device Key - set value only once"               , device_key_set_value_only_once_test               , greentea_failure_handler),
                 Case("Device Key - derive key consistency 16 byte key", device_key_derive_key_consistency_16_byte_key_test, greentea_failure_handler),
                 Case("Device Key - derive key consistency 32 byte key", device_key_derive_key_consistency_32_byte_key_test, greentea_failure_handler),
                 Case("Device Key - derive key key type 16"            , device_key_derive_key_key_type_16_test            , greentea_failure_handler),
                 Case("Device Key - derive key key type 32"            , device_key_derive_key_key_type_32_test            , greentea_failure_handler),
                 Case("Device Key - derive key wrong key type"         , device_key_derive_key_wrong_key_type_test         , greentea_failure_handler)
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(20, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main()
{
    return Harness::run(specification);
}

