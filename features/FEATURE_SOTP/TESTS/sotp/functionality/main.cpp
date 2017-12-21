/*
* Copyright (c) 2016 ARM Limited. All rights reserved.
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

#include "sotp.h"
#include "sotp_int_flash_wrapper.h"
#include "sotp_os_wrapper.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cmsis.h"
#include "cmsis_os2.h"
#include "mbed_rtos_storage.h"

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity/unity.h"
#include "utest/utest.h"

using namespace utest::v1;

#undef MIN
#define MIN(a,b)            ((a) < (b) ? (a) : (b))

#define SOTP_MAX_NAME_LENGTH (1024)
#define SOTP_SIZE (64 *1024)
#define MASTER_RECORD_SIZE (8 + 4)

#define MAX_DATA_SIZE 128

#define NUM_OF_ITERATIONS_CHUNK_TEST (16)
static uint32_t sotp_testing_buf_set[SOTP_MAX_NAME_LENGTH] = {0};
static uint32_t sotp_testing_buf_get[SOTP_MAX_NAME_LENGTH] = {0};

#define THR_TEST_NUM_BUFFS 10
#define THR_TEST_NUM_SECS 10
#define TEST_THREAD_STACK_SIZE (2048)

#define MAX_NUMBER_OF_THREADS 6

#ifndef SOTP_PROBE_ONLY
static uint32_t *thr_test_buffs[SOTP_MAX_TYPES][THR_TEST_NUM_BUFFS];
static uint16_t thr_test_sizes[SOTP_MAX_TYPES][THR_TEST_NUM_BUFFS];
static int thr_test_inds[SOTP_MAX_TYPES];
static int thr_test_num_threads;
static uint8_t thr_test_last_type;
static int thr_test_last_ind;
#endif

void zero_get_array(uint32_t array_size)
{
    for (uint32_t array_index = 0; array_index < ((array_size/4)+(array_size%4)); array_index++)
    {
        sotp_testing_buf_get[array_index] = 0;
    }
}


void gen_random(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}


void sotp_basic_flash_test()
{

    uint32_t pattern1[8], pattern2[6], pattern3[4], read_pat[12];
    int res;
    uint32_t address;
    sotp_area_data_t area_data;
    uint8_t area;

    sotp_int_flash_init();

    for (area = 0; area < 2; area++) {
        res = sotp_int_flash_get_area_info(area, &area_data);
        TEST_ASSERT_EQUAL(0, res);
        printf("\nArea %d data, address 0x%lx, size %d\n", area, area_data.address, area_data.size);
        address = area_data.address;

        res = sotp_int_flash_erase(area_data.address, area_data.size);
        TEST_ASSERT_EQUAL(0, res);

        memset(pattern1, 0xFF, sizeof(pattern1));
        memset(pattern1, 'A', 15);
        res = sotp_int_flash_write(15, address, pattern1);
        TEST_ASSERT_EQUAL(0, res);
        address += sizeof(pattern1);

        memset(pattern2, 0xFF, sizeof(pattern2));
        memset(pattern2, 'B', 16);
        res = sotp_int_flash_write(16, address, pattern2);
        TEST_ASSERT_EQUAL(0, res);
        address += sizeof(pattern2);

        memset(pattern3, 0xFF, sizeof(pattern3));
        memset(pattern3, 'C', 7);
        res = sotp_int_flash_write(7, address, pattern3);
        TEST_ASSERT_EQUAL(0, res);
        address += sizeof(pattern3);

        address = area_data.address;

        res = sotp_int_flash_read(sizeof(pattern1), address, read_pat);
        TEST_ASSERT_EQUAL(0, res);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t *)read_pat, (uint8_t *)pattern1, sizeof(pattern1));
        address += sizeof(pattern1);

        res = sotp_int_flash_read(sizeof(pattern2), address, read_pat);
        TEST_ASSERT_EQUAL(0, res);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t *)read_pat, (uint8_t *)pattern2, sizeof(pattern2));
        address += sizeof(pattern2);

        res = sotp_int_flash_read(sizeof(pattern3), address, read_pat);
        TEST_ASSERT_EQUAL(0, res);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t *)read_pat, (uint8_t *)pattern3, sizeof(pattern3));
        address += sizeof(pattern3);
    }

    sotp_int_flash_deinit();
}

void sotp_basic_functionality_test()
{

    sotp_int_flash_init();

    uint32_t char_index = 0;
    uint16_t actual_len_bytes = 0;
    for (uint32_t num_of_iterations = 0; num_of_iterations < SOTP_MAX_NAME_LENGTH; num_of_iterations++)
    {
        sotp_testing_buf_set[num_of_iterations] = (24 << char_index) | (16 << (char_index + 1)) | (8 << (char_index + 2)) | (char_index + 3);
        char_index += 4;
        if (char_index == 256)
        {
        	char_index = 0;
        }

    }

#ifdef SOTP_PROBE_ONLY
    sotp_result_e result = sotp_probe(SOTP_MAX_TYPES, 0, NULL, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_NOT_FOUND, result);
#else
    sotp_result_e result = sotp_reset();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);

    result = sotp_set(5, 18, sotp_testing_buf_set);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_get(5, 22, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(18, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)sotp_testing_buf_set, (uint8_t*)sotp_testing_buf_get, 15);
#ifdef SOTP_TESTING
    result = sotp_remove(5);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_get(5, 20, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_NOT_FOUND, result);
#endif

    result = sotp_set(11, 0, NULL);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(9, 20, NULL);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(7, 0, sotp_testing_buf_set);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(10, 2048, sotp_testing_buf_set);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(13, 3, &(sotp_testing_buf_set[1]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(15, 15, &(sotp_testing_buf_set[2]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(64, 15, &(sotp_testing_buf_set[2]));
    TEST_ASSERT_EQUAL(SOTP_BAD_VALUE, result);
    result = sotp_set(9, 20, &(sotp_testing_buf_set[3]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_get(14, 20, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_NOT_FOUND, result);
    result = sotp_get(7, 0, NULL, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);
    result = sotp_get(7, 15, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);
    result = sotp_get(7, 0, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);
    result = sotp_get(9, 0, NULL, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_BUFF_TOO_SMALL, result);
    result = sotp_get(9, 150, NULL, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_BUFF_TOO_SMALL, result);
    result = sotp_get(9, 0, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_BUFF_TOO_SMALL, result);
    result = sotp_get(10, 2048, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(2048, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)sotp_testing_buf_set, (uint8_t*)sotp_testing_buf_get, 2048);
    zero_get_array(2048);
    result = sotp_get(10, 2049, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(2048, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)sotp_testing_buf_set, (uint8_t*)sotp_testing_buf_get, 2048);
    zero_get_array(2048);
    result = sotp_get(10, 2047, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_BUFF_TOO_SMALL, result);
    actual_len_bytes = 0;
    result = sotp_get(64, 20, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_BAD_VALUE, result);
    result = sotp_get(9, 20, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(20, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[3]), (uint8_t*)sotp_testing_buf_get, 20);
    zero_get_array(20);
    result = sotp_get(9, 21, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(20, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[3]), (uint8_t*)sotp_testing_buf_get, 20);
    zero_get_array(20);
    result = sotp_get(9, 19, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_BUFF_TOO_SMALL, result);
    result = sotp_get(13, 3, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(3, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[1]), (uint8_t*)sotp_testing_buf_get, 3);
    result = sotp_get_item_size(13, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(3, actual_len_bytes);
    zero_get_array(3);
    result = sotp_get(13, 4, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(3, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[1]), (uint8_t*)sotp_testing_buf_get, 3);
    zero_get_array(3);
    result = sotp_get(13, 2, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_BUFF_TOO_SMALL, result);
    result = sotp_init();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    // check all the expected types
    actual_len_bytes = 0;
    result = sotp_get(10, 2048, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(2048, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)sotp_testing_buf_set, (uint8_t*)sotp_testing_buf_get, 2048);
    zero_get_array(2048);

    actual_len_bytes = 0;
    result = sotp_get(11, 2048, sotp_testing_buf_get, &actual_len_bytes); 
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);

    actual_len_bytes = 0;
    result = sotp_get(13, 3, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(3, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[1]), (uint8_t*)sotp_testing_buf_get, 3);
    zero_get_array(3);

    actual_len_bytes = 0;
    result = sotp_get(9, 20, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(20, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[3]), (uint8_t*)sotp_testing_buf_get, 20);
    zero_get_array(20);

    actual_len_bytes = 0;
    result = sotp_get(7, 0, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);

    actual_len_bytes = 0;
    result = sotp_get(15, 15, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(15, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[2]), (uint8_t*)sotp_testing_buf_get, 15);
    zero_get_array(15);

    // change the data for all types
    result = sotp_set(10, 15, &(sotp_testing_buf_set[4]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(11, 27, &(sotp_testing_buf_set[5]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(13, 7, &(sotp_testing_buf_set[6]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(9, 0, &(sotp_testing_buf_set[7]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(7, 48, &(sotp_testing_buf_set[8]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(14, 109, &(sotp_testing_buf_set[9]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    result = sotp_set(15, 53, &(sotp_testing_buf_set[10]));
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);

#ifdef SOTP_TESTING
    result = sotp_force_garbage_collection();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
#endif

    actual_len_bytes = 0;
    result = sotp_get(10, 15, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(15, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[4]), (uint8_t*)sotp_testing_buf_get, 15);
    zero_get_array(15);

    actual_len_bytes = 0;
    result = sotp_get(11, 27, sotp_testing_buf_get, &actual_len_bytes); // no care about the buf and len values
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(27, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[5]), (uint8_t*)sotp_testing_buf_get, 27);
    zero_get_array(27);

    actual_len_bytes = 0;
    result = sotp_get(13, 7, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(7, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[6]), (uint8_t*)sotp_testing_buf_get, 7);
    zero_get_array(7);

    actual_len_bytes = 0;
    result = sotp_get(9, 0, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);

    actual_len_bytes = 0;
    result = sotp_get(7, 48, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(48, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[8]), (uint8_t*)sotp_testing_buf_get, 48);
    zero_get_array(48);

    actual_len_bytes = 0;
    result = sotp_get(14, 109, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(109, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[9]), (uint8_t*)sotp_testing_buf_get, 109);
    zero_get_array(109);

    actual_len_bytes = 0;
    result = sotp_get(15, 53, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(53, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[10]), (uint8_t*)sotp_testing_buf_get, 53);
    zero_get_array(53);

    result = sotp_deinit();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);

#if defined(SOTP_PROBE_ONLY) || defined(SOTP_TESTING)
    actual_len_bytes = 0;
    result = sotp_probe(10, 15, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(15, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[4]), (uint8_t*)sotp_testing_buf_get, 15);
#endif

    result = sotp_init();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);

    actual_len_bytes = 0;
    result = sotp_get(10, 15, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(15, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[4]), (uint8_t*)sotp_testing_buf_get, 15);
    zero_get_array(15);

    actual_len_bytes = 0;
    result = sotp_get(11, 27, sotp_testing_buf_get, &actual_len_bytes); // no care about the buf and len values
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(27, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[5]), (uint8_t*)sotp_testing_buf_get, 27);
    zero_get_array(27);

    actual_len_bytes = 0;
    result = sotp_get(13, 7, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(7, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[6]), (uint8_t*)sotp_testing_buf_get, 7);
    zero_get_array(7);

    actual_len_bytes = 0;
    result = sotp_get(9, 0, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(0, actual_len_bytes);

    actual_len_bytes = 0;
    result = sotp_get(7, 48, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(48, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[8]), (uint8_t*)sotp_testing_buf_get, 48);
    zero_get_array(48);

    actual_len_bytes = 0;
    result = sotp_get(14, 109, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(109, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[9]), (uint8_t*)sotp_testing_buf_get, 109);
    zero_get_array(109);

    actual_len_bytes = 0;
    result = sotp_get(15, 53, sotp_testing_buf_get, &actual_len_bytes);
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    TEST_ASSERT_EQUAL(53, actual_len_bytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)&(sotp_testing_buf_set[10]), (uint8_t*)sotp_testing_buf_get, 53);
    zero_get_array(53);
#endif

    sotp_int_flash_deinit();
}


void sotp_chunk_iterations_test()
{
    sotp_int_flash_init();

#ifndef SOTP_PROBE_ONLY
    uint32_t *data_array[SOTP_MAX_TYPES];
    uint32_t data_size_array[SOTP_MAX_TYPES] = {0};
    char *random_str;
    uint16_t actual_len_bytes = 0;
    sotp_result_e result = SOTP_SUCCESS;
    for (uint32_t i = 0; i <SOTP_MAX_TYPES; i++)
    {
        data_array[i] = (uint32_t *) malloc(MAX_DATA_SIZE);
    }
    random_str = (char *) malloc(MAX_DATA_SIZE+1);

    result = sotp_reset();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
    for (uint32_t iter_num_index = 0; iter_num_index < NUM_OF_ITERATIONS_CHUNK_TEST; iter_num_index++)
    {
        for (uint32_t i = 0; i <SOTP_MAX_TYPES; i++)
        {
            for (uint32_t j = 0; j < MAX_DATA_SIZE/sizeof(uint32_t); j++)
            {
                data_array[i][j] = 0;
            }
            data_size_array[i] = 0;
        }
        for (uint32_t i = 0; i < 50; i++)
        {
            uint32_t data_size = 1 + (rand() % MAX_DATA_SIZE);
            uint16_t type = rand() % SOTP_MAX_TYPES;
            gen_random(random_str , data_size);
            random_str[data_size] = '\0';
            strncpy((char*)data_array[type], random_str, data_size);
            result = sotp_set(type, data_size, data_array[type]);
            TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
            data_size_array[type] = data_size;
        }

        for (uint32_t i = 0; i < SOTP_MAX_TYPES; i++)
        {
            // check if we random this type
            if (data_size_array[i] != 0)
            {
                result = sotp_get(i, data_size_array[i], sotp_testing_buf_get, &actual_len_bytes);
                TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
                TEST_ASSERT_EQUAL(data_size_array[i], actual_len_bytes);
                TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[i]), (uint8_t*)sotp_testing_buf_get, data_size_array[i]);
            }
        }

    }
    for (uint32_t i = 0; i < SOTP_MAX_TYPES; i++)
    {
        free(data_array[i]);
    }
    free(random_str);
#endif
    sotp_int_flash_deinit();
}



void sotp_garbage_collection_test()
{
    sotp_int_flash_init();
#ifndef SOTP_PROBE_ONLY

    uint32_t sotp_curr_size = MASTER_RECORD_SIZE;
    sotp_result_e result;
    result = sotp_reset();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
#ifdef SOTP_TESTING
    result = sotp_force_garbage_collection();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
#endif
    uint32_t *data_array[SOTP_MAX_TYPES];
    uint32_t data_size_array[SOTP_MAX_TYPES] = {0};
    char *random_str;
    uint16_t actual_len_bytes = 0;

    for (uint32_t i = 0; i < SOTP_MAX_TYPES; i++)
    {
        data_array[i] = (uint32_t *) malloc(MAX_DATA_SIZE);
        for (uint32_t j = 0; j < MAX_DATA_SIZE/sizeof(uint32_t); j++)
        {
            data_array[i][j] = 0;
        }
    }
    random_str = (char *) malloc(MAX_DATA_SIZE+1);

    while (sotp_curr_size < (1.5 * SOTP_SIZE))
    {
        uint32_t data_size = 1 + (rand() % MAX_DATA_SIZE);
        uint16_t type = rand() % SOTP_MAX_TYPES;
        gen_random(random_str , data_size);
        random_str[data_size] = '\0';
        strncpy((char*)data_array[type], random_str, data_size);
        result = sotp_set(type, data_size, data_array[type]);
        TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
        data_size_array[type] = data_size;
        result = sotp_get(type, data_size_array[type], sotp_testing_buf_get, &actual_len_bytes);
        TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
        TEST_ASSERT_EQUAL(data_size_array[type], actual_len_bytes);
        TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[type]), (uint8_t*)sotp_testing_buf_get, data_size_array[type]);
        sotp_curr_size += (8 + data_size_array[type]);
    }

    for (uint32_t i = 0; i < SOTP_MAX_TYPES; i++)
    {
        if (data_size_array[i] != 0)
        {
            result = sotp_get(i, data_size_array[i], sotp_testing_buf_get, &actual_len_bytes);
            TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
            TEST_ASSERT_EQUAL(data_size_array[i], actual_len_bytes);
            TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[i]), (uint8_t*)sotp_testing_buf_get, data_size_array[i]);
        }
    }

#ifdef SOTP_TESTING
    result = sotp_force_garbage_collection();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
#endif

    for (uint32_t i = 0; i < SOTP_MAX_TYPES; i++)
    {
        // check if we random this type
        if (data_size_array[i] != 0)
        {
            result = sotp_get(i, data_size_array[i], sotp_testing_buf_get, &actual_len_bytes);
            TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
            TEST_ASSERT_EQUAL(data_size_array[i], actual_len_bytes);
            TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[i]), (uint8_t*)sotp_testing_buf_get, data_size_array[i]);
        }
    }

    result = sotp_init();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);

    for (uint32_t i = 0; i < SOTP_MAX_TYPES; i++)
    {
        // check if we random this type
        if (data_size_array[i] != 0)
        {
            result = sotp_get(i, data_size_array[i], sotp_testing_buf_get, &actual_len_bytes);
            TEST_ASSERT_EQUAL(SOTP_SUCCESS, result);
            TEST_ASSERT_EQUAL(data_size_array[i], actual_len_bytes);
            TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t*)(data_array[i]), (uint8_t*)sotp_testing_buf_get, data_size_array[i]);
        }
    }

    for (uint32_t i = 0; i < SOTP_MAX_TYPES; i++)
    {
        free(data_array[i]);
    }
    free(random_str);

#endif
    sotp_int_flash_deinit();
}

static void thread_test_check_type(int thread, uint8_t type, int check_probe)
{
    uint32_t get_buff[MAX_DATA_SIZE/sizeof(uint32_t)];
    sotp_result_e ret;
    uint16_t actual_len_bytes;
    int i, first, last;

    if (check_probe) {
#if defined(SOTP_PROBE_ONLY) || defined(SOTP_TESTING)
        ret = sotp_probe(type, MAX_DATA_SIZE, get_buff, &actual_len_bytes);
#else
        ret = sotp_get(type, MAX_DATA_SIZE, get_buff, &actual_len_bytes);
#endif
    }
    else {
        ret = sotp_get(type, MAX_DATA_SIZE, get_buff, &actual_len_bytes);
    }
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, ret);
    TEST_ASSERT_NOT_EQUAL(0, actual_len_bytes);

    if (thr_test_num_threads == 1) {
        first = thr_test_inds[type];
        last = thr_test_inds[type];
    }
    else {
        first = 0;
        last = THR_TEST_NUM_BUFFS - 1;
    }

    for (i = first; i <= last; i++) {
        if (thr_test_sizes[type][i] != actual_len_bytes)
            continue;

        if (!memcmp(thr_test_buffs[type][i], get_buff, actual_len_bytes))
            return;
    }

    if (type == thr_test_last_type) {
        if ((thr_test_sizes[type][thr_test_last_ind] == actual_len_bytes) &&
            (!memcmp(thr_test_buffs[type][thr_test_last_ind], get_buff, actual_len_bytes)))
            return;
    }

    // Got here - always assert
    TEST_ASSERT(0);
}

void thread_test_worker(void *arg)
{
    sotp_result_e ret;
    int buf_num, is_set;
    uint8_t type;
    int thr = *(int *) arg;

    for (;;) {
        type = rand() % SOTP_MAX_TYPES;
        is_set = rand() % 4;

        if (is_set) {
            buf_num = rand() % THR_TEST_NUM_BUFFS;
            thr_test_last_type = type;
            thr_test_last_ind = buf_num;
            ret = sotp_set(type, thr_test_sizes[type][buf_num], thr_test_buffs[type][buf_num]);
            TEST_ASSERT_EQUAL(SOTP_SUCCESS, ret);
            thr_test_inds[type] = buf_num;
        }
        else
            thread_test_check_type(thr, type, 0);

        osDelay(1);
    }
}

static void run_thread_test(int num_threads)
{
#ifndef SOTP_PROBE_ONLY

    uint8_t *ptr;
    int i, j;
    uint16_t size, max_size;
    uint8_t type;
    sotp_result_e ret;
    int thr_nums[MAX_NUMBER_OF_THREADS];
    mbed_rtos_storage_thread_t test_thread_tcb[MAX_NUMBER_OF_THREADS];
    osThreadAttr_t test_thread_attr;
    const char *thr_name = "sotp_test_thread";
    osThreadId_t test_thread_id[MAX_NUMBER_OF_THREADS];
    int res;
    sotp_area_data_t area_data[SOTP_INT_FLASH_NUM_AREAS];
    uint8_t area;
    uint8_t *stacks;

    sotp_int_flash_init();
    ret = sotp_reset();
    TEST_ASSERT_EQUAL(SOTP_SUCCESS, ret);

    for (area = 0; area < SOTP_INT_FLASH_NUM_AREAS; area++) {
        res = sotp_int_flash_get_area_info(area, &area_data[area]);
        TEST_ASSERT_EQUAL(0, res);
    }

    max_size = MIN(area_data[0].size, area_data[1].size);
    max_size = MIN(max_size / SOTP_MAX_TYPES - 16, max_size);
    max_size = MIN(max_size, MAX_DATA_SIZE);

    for (type = 0; type < SOTP_MAX_TYPES; type++) {
        for (i = 0; i < THR_TEST_NUM_BUFFS; i++) {
            size = 1 + rand() % max_size;
            thr_test_sizes[type][i] = size;
            thr_test_buffs[type][i] = (uint32_t*) malloc(size);
            thr_test_inds[type] = 0;
            ptr = (uint8_t *) thr_test_buffs[type][i];
            for (j = 0; j < size; j++)
                ptr[j] = (uint8_t) (rand() % 256);
        }
        ret = sotp_set(type, thr_test_sizes[type][0], thr_test_buffs[type][0]);
        TEST_ASSERT_EQUAL(SOTP_SUCCESS, ret);
    }

    stacks = (uint8_t *) malloc(num_threads * sizeof(TEST_THREAD_STACK_SIZE));
    TEST_ASSERT_NOT_EQUAL(NULL, stacks);

    for (i = 0; i < num_threads; i++) {
        thr_nums[i] = i;
        test_thread_attr.name = thr_name;
        test_thread_attr.cb_mem = &test_thread_tcb[i];
        test_thread_attr.stack_mem = &stacks[i * sizeof(TEST_THREAD_STACK_SIZE)];
        test_thread_attr.priority = (osPriority_t) ((int)osPriorityBelowNormal + i);
        test_thread_attr.stack_size = TEST_THREAD_STACK_SIZE;
        test_thread_attr.cb_size = sizeof(mbed_rtos_storage_thread_t);

        test_thread_id[i] = osThreadNew(thread_test_worker, &thr_nums[i], &test_thread_attr);
        TEST_ASSERT_NOT_EQUAL(NULL, test_thread_id[i]);
    }

    osDelay(THR_TEST_NUM_SECS * 1000);

    for (i = 0; i < num_threads; i++) {
        osThreadTerminate(test_thread_id[i]);
    }
    free(stacks);

    osDelay(1000);

    sotp_int_flash_deinit();
    sotp_deinit();
    sotp_int_flash_init();

    thread_test_check_type(0, SOTP_MAX_TYPES-1, 1);

    sotp_init();

    for (type = 0; type < SOTP_MAX_TYPES; type++) {
        thread_test_check_type(0, type, 0);
        TEST_ASSERT_EQUAL(SOTP_SUCCESS, ret);
    }

    for (type = 0; type < SOTP_MAX_TYPES; type++) {
        for (i = 0; i < THR_TEST_NUM_BUFFS; i++) {
            free(thr_test_buffs[type][i]);
        }
    }
    sotp_int_flash_deinit();
#endif
}


void sotp_single_thread_test()
{
    run_thread_test(1);
}

void sotp_multi_thread_test()
{
    run_thread_test(MAX_NUMBER_OF_THREADS - 1);
}


utest::v1::status_t greentea_failure_handler(const Case *const source, const failure_t reason) {
    greentea_case_failure_abort_handler(source, reason);
    return STATUS_CONTINUE;
}

Case cases[] = {
        Case("SOTP: Basic flash",          sotp_basic_flash_test,         greentea_failure_handler),
        Case("SOTP: Basic functionality",  sotp_basic_functionality_test, greentea_failure_handler),
        Case("SOTP: Chunk iterations",     sotp_chunk_iterations_test,    greentea_failure_handler),
        Case("SOTP: Garbage collection" ,  sotp_garbage_collection_test,  greentea_failure_handler),
        Case("SOTP: Single thread test",   sotp_single_thread_test,       greentea_failure_handler),
//      Case("SOTP: Multiple thread test", sotp_multi_thread_test,        greentea_failure_handler),
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(120, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main() {
    return !Harness::run(specification);
}
