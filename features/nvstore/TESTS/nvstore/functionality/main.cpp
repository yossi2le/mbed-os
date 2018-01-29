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

#include "nvstore.h"
#include "nvstore_int_flash_wrapper.h"
#include "nvstore_shared_lock.h"
#include "thread.h"
#include "greentea-client/test_env.h"
#include "unity/unity.h"
#include "utest/utest.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef NVSTORE_ENABLED
#error [NOT_SUPPORTED] NVSTORE needs to be enabled for this test
#endif

using namespace utest::v1;

#undef MIN
#define MIN(a,b)            ((a) < (b) ? (a) : (b))

#define MAX_KEYS 20

#define NVSTORE_MAX_NAME_LENGTH (1024)
#define NVSTORE_SIZE (64 *1024)
#define MASTER_RECORD_SIZE (8 + 4)

#define MAX_DATA_SIZE 128

#define NUM_OF_ITERATIONS_CHUNK_TEST (16)
static uint32_t nvstore_testing_buf_set[NVSTORE_MAX_NAME_LENGTH] = {0};
static uint32_t nvstore_testing_buf_get[NVSTORE_MAX_NAME_LENGTH] = {0};

#define THR_TEST_NUM_BUFFS 10
#define THR_TEST_NUM_SECS 10
#define TEST_THREAD_STACK_SIZE (2048)

#define MAX_NUMBER_OF_THREADS 4

static uint32_t *thr_test_buffs[MAX_KEYS][THR_TEST_NUM_BUFFS];
static uint16_t thr_test_sizes[MAX_KEYS][THR_TEST_NUM_BUFFS];
static int thr_test_inds[MAX_KEYS];
static int thr_test_num_threads;
static uint8_t thr_test_last_key;
static int thr_test_last_ind;

void zero_get_array(uint32_t array_size)
{
    memset(nvstore_testing_buf_get, 0, array_size);
}


void gen_random(uint8_t *s, int len) {
    for (int i = 0; i < len; ++i) {
        s[i] = rand() % 256;
    }
}


typedef struct
{
    uint32_t address;
    size_t   size;
} nvstore_area_data_t;

const nvstore_area_data_t flash_area_params[] =
{
        {NVSTORE_AREA_1_ADDRESS, NVSTORE_AREA_1_SIZE},
        {NVSTORE_AREA_2_ADDRESS, NVSTORE_AREA_2_SIZE}
};


void nvstore_basic_flash_test()
{

    uint32_t pattern1[8], pattern2[6], pattern3[4], read_pat[12];
    int res;
    uint32_t address;
    uint8_t area;

    nvstore_int_flash_init();

    for (area = 0; area < 2; area++) {
        printf("\nArea %d data, address 0x%lx, size %d\n", area, flash_area_params[area].address, flash_area_params[area].size);
        address = flash_area_params[area].address;

        res = nvstore_int_flash_erase(flash_area_params[area].address, flash_area_params[area].size);
        TEST_ASSERT_EQUAL(0, res);

        memset(pattern1, 0xFF, sizeof(pattern1));
        memset(pattern1, 'A', 15);
        res = nvstore_int_flash_write(15, address, pattern1);
        TEST_ASSERT_EQUAL(0, res);
        address += sizeof(pattern1);

        memset(pattern2, 0xFF, sizeof(pattern2));
        memset(pattern2, 'B', 16);
        res = nvstore_int_flash_write(16, address, pattern2);
        TEST_ASSERT_EQUAL(0, res);
        address += sizeof(pattern2);

        memset(pattern3, 0xFF, sizeof(pattern3));
        memset(pattern3, 'C', 7);
        res = nvstore_int_flash_write(7, address, pattern3);
        TEST_ASSERT_EQUAL(0, res);
        address += sizeof(pattern3);

        address = flash_area_params[area].address;

        res = nvstore_int_flash_read(sizeof(pattern1), address, read_pat);
        TEST_ASSERT_EQUAL(0, res);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t *)read_pat, (uint8_t *)pattern1, sizeof(pattern1));
        address += sizeof(pattern1);

        res = nvstore_int_flash_read(sizeof(pattern2), address, read_pat);
        TEST_ASSERT_EQUAL(0, res);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t *)read_pat, (uint8_t *)pattern2, sizeof(pattern2));
        address += sizeof(pattern2);

        res = nvstore_int_flash_read(sizeof(pattern3), address, read_pat);
        TEST_ASSERT_EQUAL(0, res);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t *)read_pat, (uint8_t *)pattern3, sizeof(pattern3));
        address += sizeof(pattern3);
    }

    nvstore_int_flash_deinit();
}

void nvstore_basic_functionality_test()
{

    nvstore_int_flash_init();

    uint16_t actual_len_bytes = 0;
    NVStore &nvstore = NVStore::get_instance();

    int result;

    gen_random((uint8_t *) nvstore_testing_buf_set, NVSTORE_MAX_NAME_LENGTH);

    result = nvstore.probe(MAX_KEYS, 0, NULL, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_NOT_FOUND, result);

    nvstore.set_max_keys(MAX_KEYS);
    TEST_ASSERT_EQUAL(MAX_KEYS, nvstore.get_max_keys());

    result = nvstore.reset();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);

    result = nvstore.set(5, 18, nvstore_testing_buf_set);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);

    result = nvstore.get(5, 22, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(18, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)nvstore_testing_buf_set, (uint8_t*)nvstore_testing_buf_get, 15);
    result = nvstore.remove(5);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.get(5, 20, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_NOT_FOUND, result);

    result = nvstore.set(11, 0, NULL);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(9, 20, NULL);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(7, 0, nvstore_testing_buf_set);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(10, 2048, nvstore_testing_buf_set);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(13, 3, &(nvstore_testing_buf_set[1]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(15, 15, &(nvstore_testing_buf_set[2]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(64, 15, &(nvstore_testing_buf_set[2]));
    TEST_ASSERT_EQUAL(NVSTORE_BAD_VALUE, result);
    result = nvstore.set(9, 20, &(nvstore_testing_buf_set[3]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set_once(19, 12, &(nvstore_testing_buf_set[2]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(19, 10, &(nvstore_testing_buf_set[3]));
    TEST_ASSERT_EQUAL(NVSTORE_ALREADY_EXISTS, result);

    // Make sure set items are also gotten OK after reset
    result = nvstore.deinit();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.init();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);

    result = nvstore.get(14, 20, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_NOT_FOUND, result);
    result = nvstore.get(7, 0, NULL, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);
    result = nvstore.get(7, 15, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);
    result = nvstore.get(7, 0, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);
    result = nvstore.get(9, 0, NULL, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_BUFF_TOO_SMALL, result);
    result = nvstore.get(9, 150, NULL, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_BUFF_TOO_SMALL, result);
    result = nvstore.get(9, 0, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_BUFF_TOO_SMALL, result);
    result = nvstore.get(10, 2048, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(2048, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)nvstore_testing_buf_set, (uint8_t*)nvstore_testing_buf_get, 2048);
    zero_get_array(2048);
    result = nvstore.get(10, 2049, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(2048, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)nvstore_testing_buf_set, (uint8_t*)nvstore_testing_buf_get, 2048);
    zero_get_array(2048);
    result = nvstore.get(10, 2047, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_BUFF_TOO_SMALL, result);
    actual_len_bytes = 0;
    result = nvstore.get(64, 20, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_BAD_VALUE, result);
    result = nvstore.get(9, 20, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(20, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[3]), (uint8_t*)nvstore_testing_buf_get, 20);
    zero_get_array(20);
    result = nvstore.get(9, 21, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(20, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[3]), (uint8_t*)nvstore_testing_buf_get, 20);
    zero_get_array(20);
    result = nvstore.get(9, 19, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_BUFF_TOO_SMALL, result);
    result = nvstore.get(13, 3, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(3, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[1]), (uint8_t*)nvstore_testing_buf_get, 3);
    result = nvstore.get_item_size(13, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(3, actual_len_bytes);
    zero_get_array(3);
    result = nvstore.get(13, 4, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(3, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[1]), (uint8_t*)nvstore_testing_buf_get, 3);
    zero_get_array(3);
    result = nvstore.get(13, 2, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_BUFF_TOO_SMALL, result);
    result = nvstore.init();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    // check all the expected keys
    actual_len_bytes = 0;
    result = nvstore.get(10, 2048, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(2048, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)nvstore_testing_buf_set, (uint8_t*)nvstore_testing_buf_get, 2048);
    zero_get_array(2048);

    actual_len_bytes = 0;
    result = nvstore.get(11, 2048, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);

    actual_len_bytes = 0;
    result = nvstore.get(13, 3, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(3, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[1]), (uint8_t*)nvstore_testing_buf_get, 3);
    zero_get_array(3);

    actual_len_bytes = 0;
    result = nvstore.get(9, 20, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(20, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[3]), (uint8_t*)nvstore_testing_buf_get, 20);
    zero_get_array(20);

    actual_len_bytes = 0;
    result = nvstore.get(7, 0, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);

    actual_len_bytes = 0;
    result = nvstore.get(15, 15, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(15, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[2]), (uint8_t*)nvstore_testing_buf_get, 15);
    zero_get_array(15);

    actual_len_bytes = 0;
    result = nvstore.get(19, 12, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(12, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[2]), (uint8_t*)nvstore_testing_buf_get, 12);
    zero_get_array(12);

    // change the data for all keys
    result = nvstore.set(10, 15, &(nvstore_testing_buf_set[4]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(11, 27, &(nvstore_testing_buf_set[5]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(13, 7, &(nvstore_testing_buf_set[6]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(9, 0, &(nvstore_testing_buf_set[7]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(7, 48, &(nvstore_testing_buf_set[8]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(14, 109, &(nvstore_testing_buf_set[9]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    result = nvstore.set(15, 53, &(nvstore_testing_buf_set[10]));
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);

#ifdef NVSTORE_TESTING
    result = nvstore.force_garbage_collection();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
#endif

    actual_len_bytes = 0;
    result = nvstore.get(10, 15, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(15, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[4]), (uint8_t*)nvstore_testing_buf_get, 15);
    zero_get_array(15);

    actual_len_bytes = 0;
    result = nvstore.get(11, 27, nvstore_testing_buf_get, actual_len_bytes); // no care about the buf and len values
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(27, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[5]), (uint8_t*)nvstore_testing_buf_get, 27);
    zero_get_array(27);

    actual_len_bytes = 0;
    result = nvstore.get(13, 7, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(7, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[6]), (uint8_t*)nvstore_testing_buf_get, 7);
    zero_get_array(7);

    actual_len_bytes = 0;
    result = nvstore.get(9, 0, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);

    actual_len_bytes = 0;
    result = nvstore.get(7, 48, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(48, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[8]), (uint8_t*)nvstore_testing_buf_get, 48);
    zero_get_array(48);

    actual_len_bytes = 0;
    result = nvstore.get(14, 109, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(109, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[9]), (uint8_t*)nvstore_testing_buf_get, 109);
    zero_get_array(109);

    actual_len_bytes = 0;
    result = nvstore.get(15, 53, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(53, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[10]), (uint8_t*)nvstore_testing_buf_get, 53);
    zero_get_array(53);

    result = nvstore.deinit();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);

    actual_len_bytes = 0;
    result = nvstore.probe(10, 15, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(15, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[4]), (uint8_t*)nvstore_testing_buf_get, 15);

    result = nvstore.init();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);

    actual_len_bytes = 0;
    result = nvstore.get(10, 15, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(15, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[4]), (uint8_t*)nvstore_testing_buf_get, 15);
    zero_get_array(15);

    actual_len_bytes = 0;
    result = nvstore.get(11, 27, nvstore_testing_buf_get, actual_len_bytes); // no care about the buf and len values
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(27, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[5]), (uint8_t*)nvstore_testing_buf_get, 27);
    zero_get_array(27);

    actual_len_bytes = 0;
    result = nvstore.get(13, 7, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(7, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[6]), (uint8_t*)nvstore_testing_buf_get, 7);
    zero_get_array(7);

    actual_len_bytes = 0;
    result = nvstore.get(9, 0, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);

    actual_len_bytes = 0;
    result = nvstore.get(7, 48, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(48, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[8]), (uint8_t*)nvstore_testing_buf_get, 48);
    zero_get_array(48);

    actual_len_bytes = 0;
    result = nvstore.get(14, 109, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(109, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[9]), (uint8_t*)nvstore_testing_buf_get, 109);
    zero_get_array(109);

    actual_len_bytes = 0;
    result = nvstore.get(15, 53, nvstore_testing_buf_get, actual_len_bytes);
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    TEST_ASSERT_EQUAL(53, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(nvstore_testing_buf_set[10]), (uint8_t*)nvstore_testing_buf_get, 53);
    zero_get_array(53);

    nvstore_int_flash_deinit();
}


void nvstore_chunk_iterations_test()
{
    nvstore_int_flash_init();

    uint32_t *data_array[MAX_KEYS];
    uint32_t data_size_array[MAX_KEYS] = {0};
    uint16_t actual_len_bytes = 0;
    int result = NVSTORE_SUCCESS;
    NVStore &nvstore = NVStore::get_instance();

    for (uint16_t i = 0; i < MAX_KEYS; i++)
    {
        data_array[i] = (uint32_t *) malloc(MAX_DATA_SIZE);
    }

    result = nvstore.reset();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
    for (uint32_t iter_num_index = 0; iter_num_index < NUM_OF_ITERATIONS_CHUNK_TEST; iter_num_index++)
    {
        memset(data_size_array, 0, sizeof(data_size_array));
        for (uint32_t i = 0; i < 50; i++)
        {
            uint32_t data_size = 1 + (rand() % MAX_DATA_SIZE);
            uint16_t key = rand() % MAX_KEYS;
            gen_random((uint8_t *)data_array[key] , data_size);
            result = nvstore.set(key, data_size, data_array[key]);
            TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
            data_size_array[key] = data_size;
        }

        for (uint16_t i = 0; i < MAX_KEYS; i++)
        {
            if (data_size_array[i] != 0)
            {
                result = nvstore.get(i, data_size_array[i], nvstore_testing_buf_get, actual_len_bytes);
                TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
                TEST_ASSERT_EQUAL(data_size_array[i], actual_len_bytes);
                TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[i]), (uint8_t*)nvstore_testing_buf_get, data_size_array[i]);
            }
        }

    }
    for (uint16_t i = 0; i < MAX_KEYS; i++)
    {
        free(data_array[i]);
    }
    nvstore_int_flash_deinit();
}



void nvstore_garbage_collection_test()
{
    nvstore_int_flash_init();

    uint32_t nvstore_curr_size = MASTER_RECORD_SIZE;
    int result;
    NVStore &nvstore = NVStore::get_instance();

    result = nvstore.reset();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
#ifdef NVSTORE_TESTING
    result = nvstore.force_garbage_collection();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
#endif
    uint32_t *data_array[MAX_KEYS];
    uint32_t data_size_array[MAX_KEYS] = {0};
    uint16_t actual_len_bytes = 0;

    for (uint32_t i = 0; i < MAX_KEYS; i++)
    {
        data_array[i] = (uint32_t *) malloc(MAX_DATA_SIZE);
    }

    while (nvstore_curr_size < (1.5 * NVSTORE_SIZE))
    {
        uint32_t data_size = 1 + (rand() % MAX_DATA_SIZE);
        uint16_t key = rand() % MAX_KEYS;
        gen_random((uint8_t *)data_array[key], data_size);
        result = nvstore.set(key, data_size, data_array[key]);
        TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
        data_size_array[key] = data_size;
        result = nvstore.get(key, data_size_array[key], nvstore_testing_buf_get, actual_len_bytes);
        TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
        TEST_ASSERT_EQUAL(data_size_array[key], actual_len_bytes);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[key]), (uint8_t*)nvstore_testing_buf_get, data_size_array[key]);
        nvstore_curr_size += (8 + data_size_array[key]);
    }

    for (uint16_t i = 0; i < MAX_KEYS; i++)
    {
        if (data_size_array[i] != 0)
        {
            result = nvstore.get(i, data_size_array[i], nvstore_testing_buf_get, actual_len_bytes);
            TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
            TEST_ASSERT_EQUAL(data_size_array[i], actual_len_bytes);
            TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[i]), (uint8_t*)nvstore_testing_buf_get, data_size_array[i]);
        }
    }

#ifdef NVSTORE_TESTING
    result = nvstore.force_garbage_collection();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
#endif

    for (uint16_t i = 0; i < MAX_KEYS; i++)
    {
        if (data_size_array[i] != 0)
        {
            result = nvstore.get(i, data_size_array[i], nvstore_testing_buf_get, actual_len_bytes);
            TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
            TEST_ASSERT_EQUAL(data_size_array[i], actual_len_bytes);
            TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[i]), (uint8_t*)nvstore_testing_buf_get, data_size_array[i]);
        }
    }

    result = nvstore.init();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);

    for (uint16_t i = 0; i < MAX_KEYS; i++)
    {
        if (data_size_array[i] != 0)
        {
            result = nvstore.get(i, data_size_array[i], nvstore_testing_buf_get, actual_len_bytes);
            TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, result);
            TEST_ASSERT_EQUAL(data_size_array[i], actual_len_bytes);
            TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[i]), (uint8_t*)nvstore_testing_buf_get, data_size_array[i]);
        }
    }

    for (uint16_t i = 0; i < MAX_KEYS; i++)
    {
        free(data_array[i]);
    }

    nvstore_int_flash_deinit();
}

static void thread_test_check_key(uint16_t key, int check_probe)
{
    uint32_t get_buff[MAX_DATA_SIZE/sizeof(uint32_t)];
    int ret;
    uint16_t actual_len_bytes;
    int i, first, last;
    NVStore &nvstore = NVStore::get_instance();

    if (check_probe) {
        ret = nvstore.probe(key, MAX_DATA_SIZE, get_buff, actual_len_bytes);
    }
    else {
        ret = nvstore.get(key, MAX_DATA_SIZE, get_buff, actual_len_bytes);
    }
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, ret);
    TEST_ASSERT_NOT_EQUAL(0, actual_len_bytes);

    if (thr_test_num_threads == 1) {
        first = thr_test_inds[key];
        last = thr_test_inds[key];
    }
    else {
        first = 0;
        last = THR_TEST_NUM_BUFFS - 1;
    }

    for (i = first; i <= last; i++) {
        if (thr_test_sizes[key][i] != actual_len_bytes)
            continue;

        if (!memcmp(thr_test_buffs[key][i], get_buff, actual_len_bytes))
            return;
    }

    if (key == thr_test_last_key) {
        if ((thr_test_sizes[key][thr_test_last_ind] == actual_len_bytes) &&
            (!memcmp(thr_test_buffs[key][thr_test_last_ind], get_buff, actual_len_bytes)))
            return;
    }

    // Got here - always assert
    TEST_ASSERT(0);
}

void thread_test_worker()
{
    int ret;
    int buf_num, is_set;
    uint16_t key;
    NVStore &nvstore = NVStore::get_instance();

    for (;;) {
        key = rand() % MAX_KEYS;
        is_set = rand() % 4;

        if (is_set) {
            buf_num = rand() % THR_TEST_NUM_BUFFS;
            thr_test_last_key = key;
            thr_test_last_ind = buf_num;
            ret = nvstore.set(key, thr_test_sizes[key][buf_num], thr_test_buffs[key][buf_num]);
            TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, ret);
            thr_test_inds[key] = buf_num;
        }
        else
            thread_test_check_key(key, 0);

        osDelay(1);
    }
}

static void run_thread_test(int num_threads)
{
    int i;
    uint16_t size, max_size;
    uint16_t key;
    int ret;
    rtos::Thread **threads = new rtos::Thread*[num_threads];

    NVStore &nvstore = NVStore::get_instance();

    nvstore_int_flash_init();
    ret = nvstore.reset();
    TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, ret);

    max_size = MIN(flash_area_params[0].size, flash_area_params[1].size);
    max_size = MIN(max_size / MAX_KEYS - 16, max_size);
    max_size = MIN(max_size, MAX_DATA_SIZE);

    for (key = 0; key < MAX_KEYS; key++) {
        for (i = 0; i < THR_TEST_NUM_BUFFS; i++) {
            size = 1 + rand() % max_size;
            thr_test_sizes[key][i] = size;
            thr_test_buffs[key][i] = (uint32_t*) malloc(size);
            thr_test_inds[key] = 0;
            gen_random((uint8_t *)thr_test_buffs[key][i], size);
        }
        ret = nvstore.set(key, thr_test_sizes[key][0], thr_test_buffs[key][0]);
        TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, ret);
    }

    for (i = 0; i < num_threads; i++) {
        threads[i] = new rtos::Thread(osPriorityBelowNormal, TEST_THREAD_STACK_SIZE);
        threads[i]->start(callback(thread_test_worker));
    }

    rtos::Thread::wait(THR_TEST_NUM_SECS * 1000);

    for (i = 0; i < num_threads; i++) {
        threads[i]->terminate();
        delete threads[i];
   }

    delete[] threads;

    rtos::Thread::wait(1000);

    nvstore_int_flash_deinit();
    nvstore.deinit();
    nvstore_int_flash_init();

    thread_test_check_key(MAX_KEYS-1, 1);

    nvstore.init();

    for (key = 0; key < MAX_KEYS; key++) {
        thread_test_check_key(key, 0);
        TEST_ASSERT_EQUAL(NVSTORE_SUCCESS, ret);
    }

    for (key = 0; key < MAX_KEYS; key++) {
        for (i = 0; i < THR_TEST_NUM_BUFFS; i++) {
            free(thr_test_buffs[key][i]);
        }
    }
    nvstore_int_flash_deinit();
}


void nvstore_single_thread_test()
{
    run_thread_test(1);
}

void nvstore_multi_thread_test()
{
    run_thread_test(MAX_NUMBER_OF_THREADS);
}


utest::v1::status_t greentea_failure_handler(const Case *const source, const failure_t reason) {
    greentea_case_failure_abort_handler(source, reason);
    return STATUS_CONTINUE;
}

Case cases[] = {
        Case("NVStore: Basic flash",          nvstore_basic_flash_test,         greentea_failure_handler),
        Case("NVStore: Basic functionality",  nvstore_basic_functionality_test, greentea_failure_handler),
        Case("NVStore: Chunk iterations",     nvstore_chunk_iterations_test,    greentea_failure_handler),
        Case("NVStore: Garbage collection" ,  nvstore_garbage_collection_test,  greentea_failure_handler),
        Case("NVStore: Single thread test",   nvstore_single_thread_test,       greentea_failure_handler),
        Case("NVStore: Multiple thread test", nvstore_multi_thread_test,        greentea_failure_handler),
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(120, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main() {
    return !Harness::run(specification);
}
