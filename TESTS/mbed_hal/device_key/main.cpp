/* mbed Microcontroller Library
 * Copyright (c) 2017 ARM Limited
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

#include "mbed.h"
#include "device_key_api.h"



#ifndef DEVICE_DEVKEY
#error [NOT_SUPPORTED] DEVICE_KEY needs to be enabled for this test
#endif

using namespace utest::v1;

enum DeviceKeyType{

    DEVICE_KEY_16BYTE=16,
    DEVICE_KEY_32BYTE=32
};


//check if the size of the expected key is same us the api declare it.
void device_key_get_size_test()
{
    size_t len = device_key_get_size_in_bytes();
    TEST_ASSERT_FALSE_MESSAGE(DEVICE_KEY_16BYTE != len && DEVICE_KEY_32BYTE != len, "Device key length is not 16 or 32 Byte long");
}

void device_key_get_key_length_test()
{
    size_t expected = device_key_get_size_in_bytes();
    size_t len=expected;
    uint32_t buffer[DEVICE_KEY_32BYTE];
    memset(buffer, 0, DEVICE_KEY_32BYTE);
    int status = device_key_get_value(buffer, &len);

    TEST_ASSERT_EQUAL_INT32(0, status);

    TEST_ASSERT_EQUAL_INT32(expected, len);

}

void device_key_check_consistency_key_test()
{
    uint32_t buffer1[DEVICE_KEY_32BYTE];
    uint32_t buffer2[DEVICE_KEY_32BYTE];
    size_t len1=DEVICE_KEY_32BYTE;
    size_t len2=DEVICE_KEY_32BYTE;

    memset(buffer1, 0, DEVICE_KEY_32BYTE*sizeof(uint32_t));
    int status = device_key_get_value(buffer1, &len1);
    TEST_ASSERT_EQUAL_INT32(0, status);

    for (int i=0; i<100; i++)
    {
        memset(buffer2, 0, DEVICE_KEY_32BYTE*sizeof(uint32_t));
        status = device_key_get_value(buffer2, &len2);
        TEST_ASSERT_EQUAL_INT32(0, status);
        TEST_ASSERT_EQUAL_INT32_ARRAY(buffer1,buffer2,DEVICE_KEY_32BYTE);
        len2=DEVICE_KEY_32BYTE;
    }
}

Case cases[] = {
    Case("Device Key - get size", device_key_get_size_test),
    Case("Device Key - get key length", device_key_get_key_length_test),
    Case("Device Key - check consistency key", device_key_check_consistency_key_test)
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(20, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main()
{
    return Harness::run(specification);
}
