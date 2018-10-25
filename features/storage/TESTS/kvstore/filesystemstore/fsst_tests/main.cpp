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

#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "SPIFBlockDevice.h"
#include "LittleFileSystem.h"
#include "FATFileSystem.h"
#include "FileSystemStore.h"
#include "mbed_trace.h"
#include "rtos/Thread.h"
#include <stdlib.h>

using namespace utest::v1;
using namespace mbed;

SPIFBlockDevice bd(PTE2, PTE4, PTE1, PTE5);

typedef struct {
	int thread_num;
	FileSystemStore *fsst;
} thread_data_t;



static void test_set_thread_job(void *data)
{
    char kv_value[12] = {"valuevalue"};
    char kv_key[6] = {"key"};
    char thread_str[3] = {0};
    int err = 0;


    thread_data_t *thread_data = (thread_data_t *)data;
    int thread_num = thread_data->thread_num;
    FileSystemStore *thread_fsst = (FileSystemStore *)(thread_data->fsst);

    utest_printf("\n Thread %d Started\n", thread_num);

    strcat(kv_value,itoa(thread_num, thread_str, 10));
    strcat(kv_key,itoa(thread_num, thread_str, 10));
    err = thread_fsst->set(kv_key, kv_value, strlen(kv_value), 0x2);

    TEST_ASSERT_EQUAL(0, err);
}

void test_file_system_store_functionality_unit_test()
{
     utest_printf("Test FileSystemStore Functionality APIs..\n");

    char kv_value1[64] = {"value1value1value1value1value1value1"};
    char kv_key1[16] = {"key1"};
    char kv_value2[64] = {"value2value2value2value2value2value2"};
    char kv_key2[16] = {"key2"};
    char kv_value3[64] = {"valui3valui3"};
    char kv_key3[16] = {"kei3"};
    char kv_value5[64] = {"setonce5555"};
    char kv_key5[16] = {"key5"};
    char kv_buf[64] = {0};
    char kv_name[16] = {0};


    int err = bd.init();
    TEST_ASSERT_EQUAL(0, err);
    LittleFileSystem fs("lfs", &bd);
    //FATFileSystem fs("fatfs", &bd);

    err = fs.mount(&bd);
    if (err) {
    	err = fs.reformat(&bd);
    	TEST_ASSERT_EQUAL(0, err);
    }

    FileSystemStore *fsst = new FileSystemStore(/*10, */&fs);

    err = fsst->init();
    utest_printf("init fsst: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    err = fsst->reset();
    utest_printf("Reset! FSST, err: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    int i_ind = 0;

    size_t actual_size = 0;

    err = fsst->set(kv_key1, kv_value1, 64, 0x2/* flags */);
    utest_printf("Set1: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    err = fsst->set(kv_key2, kv_value2, strlen(kv_value2), 0x4);
    utest_printf("Set2: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    err = fsst->get(kv_key2, kv_buf, 64, &actual_size, 0/*offset*/);
    utest_printf("Get2 - err: %d, act_size: %d\n", err, (int)actual_size);
    utest_printf("Get2 - data: %s\n", kv_buf);
    TEST_ASSERT_EQUAL(0, err);

    KVStore::info_t kv_info;
    err = fsst->get_info(kv_key1, &kv_info);
    utest_printf("Get Info 1 - err: %d, flags: %d, size: %d\n", err, (int)kv_info.flags, (int)kv_info.size);
    TEST_ASSERT_EQUAL(0, err);

    err = fsst->set(kv_key3, kv_value3, 12, 0x8/* flags */);
    utest_printf("Set3: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    memset(kv_buf, 0 ,64);
    err = fsst->get(kv_key3, kv_buf, 64, &actual_size, 0/*offset*/);
    utest_printf("Get3 - err: %d, act_size: %d\n", err, (int)actual_size);
    utest_printf("Get3 - data: %s\n", kv_buf);
    TEST_ASSERT_EQUAL(0, err);

    err = fsst->set(kv_key5, kv_value5, 10, 0x1/* flags */);
    utest_printf("Set5: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    memset(kv_buf, 0 ,64);
    err = fsst->get(kv_key5, kv_buf, 64, &actual_size, 0/*offset*/);
    utest_printf("Get5 once - err: %d, act_size: %d\n", err, (int)actual_size);
    utest_printf("Get5 once - data: %s\n", kv_buf);
    TEST_ASSERT_EQUAL(0, err);

    err = fsst->set(kv_key5, kv_value3, 10, 0x8/* flags */);
    utest_printf("Set5 even though its write once: %d\n", err);
    TEST_ASSERT_EQUAL(KVSTORE_WRITE_ONCE_ERROR, err);

	memset(kv_buf, 0 ,64);
	err = fsst->get(kv_key5, kv_buf, 64, &actual_size, 0/*offset*/);
	utest_printf("Get5 once again - err: %d, act_size: %d\n", err, (int)actual_size);
	utest_printf("Get5 once again - data: %s\n", kv_buf);
	TEST_ASSERT_EQUAL(0, err);

    err = fsst->get("key4", kv_buf, 64, &actual_size, 0/*offset*/);
    utest_printf("Get Non Existing Key4 - err: %d, act_size: %d\n", err, (int)actual_size);
    TEST_ASSERT_EQUAL(KVSTORE_NOT_FOUND, err);

    utest_printf("Iterate All Files: \n");
    KVStore::iterator_t kv_it;
    err = fsst->iterator_open(&kv_it, NULL);
    TEST_ASSERT_EQUAL(0, err);
    utest_printf("Iterator Open - err: %d\n", err);
    i_ind = 0;
    while (fsst->iterator_next(kv_it, kv_name, 16) != KVSTORE_NOT_FOUND)
    {
    	i_ind++;
    	utest_printf("File: %d, key: %s\n", i_ind, kv_name);
    }
    fsst->iterator_close(kv_it);
    TEST_ASSERT_EQUAL(0, err);

    err = fsst->remove(kv_key5);
    utest_printf("Remove5 (once) - err: %d\n", err);
    TEST_ASSERT_EQUAL(KVSTORE_WRITE_ONCE_ERROR, err);

    memset(kv_buf, 0 ,64);
    err = fsst->get(kv_key5, kv_buf, 64, &actual_size, 0/*offset*/);
    utest_printf("Get5 (once) again - err: %d, act_size: %d\n", err, (int)actual_size);
    utest_printf("Get5 (once) again - data: %s\n", kv_buf);
    TEST_ASSERT_EQUAL(0, err);

    utest_printf("Iterate Files with prefix = key \n");
    fsst->iterator_open(&kv_it, "key");
    TEST_ASSERT_EQUAL(0, err);
    i_ind = 0;
    while (fsst->iterator_next(kv_it, kv_name, 16) != KVSTORE_NOT_FOUND)
    {
    	i_ind++;
    	utest_printf("File: %d, key: %s\n", i_ind, kv_name);
    }
    fsst->iterator_close(kv_it);
    TEST_ASSERT_EQUAL(0, err);

    utest_printf("Removing Name Kei3\n");
    err = fsst->remove(kv_key3);
    TEST_ASSERT_EQUAL(0, err);
    utest_printf("Remove - err: %d\n", err);
    utest_printf("(-) Remove Kei3 Again!\n");
    err = fsst->remove(kv_key3);
    TEST_ASSERT_EQUAL(KVSTORE_NOT_FOUND, err);
    utest_printf("Remove - err: %d\n", err);

    err = fsst->get(kv_key3, kv_buf, 64, &actual_size, 0);
    utest_printf("After Remove Get3 - err: %d\n", err);
    TEST_ASSERT_EQUAL(KVSTORE_NOT_FOUND, err);
    err = fsst->get_info(kv_key3, &kv_info);
    utest_printf("After Remove Get3 Info - err: %d\n", err);
    TEST_ASSERT_EQUAL(KVSTORE_NOT_FOUND, err);

    err = fsst->get(kv_key2, kv_buf, 64, &actual_size, 0/*offset*/);
    utest_printf("Before Reset Get2 - err: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);
     err = fsst->reset();
    utest_printf("Reset Status - err: %d\n", err);

    err = fsst->get(kv_key2, kv_buf, 64, &actual_size, 0/*offset*/);
    utest_printf("After Reset Get2 - err: %d\n", err);
    TEST_ASSERT_EQUAL(KVSTORE_NOT_FOUND, err);
    err = fsst->get(kv_key5, kv_buf, 64, &actual_size, 0/*offset*/);
     utest_printf("After Reset Get5 (once) - err: %d\n", err);
     TEST_ASSERT_EQUAL(KVSTORE_NOT_FOUND, err);

     err = fsst->set(kv_key5, kv_value5, 10, 0x1/* flags */);
     utest_printf("Set5: %d\n", err);
     TEST_ASSERT_EQUAL(0, err);

     memset(kv_buf, 0 ,64);
     err = fsst->get(kv_key5, kv_buf, 64, &actual_size, 0/*offset*/);
     utest_printf("Get5 once - err: %d, act_size: %d\n", err, (int)actual_size);
     utest_printf("Get5 once - data: %s\n", kv_buf);
     TEST_ASSERT_EQUAL(0, err);

    err = fsst->deinit();
    utest_printf("deinit fsst: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);
    err = bd.deinit();
    TEST_ASSERT_EQUAL(0, err);
}



void test_file_system_store_edge_cases()
{
    utest_printf("Test FileSystemStore Edge Cases..\n");
    KVStore::info_t kv_info;
    KVStore::iterator_t kv_it;
    char kv_value1[64] = {"value1value1value1value1value1value1"};
    char kv_key1[16] = {"key1"};
    char kv_value2[64] = {"value2value2value2value2value2value2"};
    char kv_buf[64] = {0};
    char kv_name[16] = {0};


    int err = bd.init();
    TEST_ASSERT_EQUAL(0, err);
    LittleFileSystem fs("lfs", &bd);
    //FATFileSystem fs("fatfs", &bd);

    err = fs.mount(&bd);
    if (err) {
    	err = fs.reformat(&bd);
    	TEST_ASSERT_EQUAL(0, err);
    }

    FileSystemStore *fsst = new FileSystemStore(/*10, */&fs);

    err = fsst->init();
    utest_printf("init fsst: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    err = fsst->reset();
    utest_printf("Reset! FSST, err: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    size_t actual_size = 0;

   /*********************************/
    /*********** Unit Test ***********/
    /*********************************/
    utest_printf("(-) Key is NULL\n");
    err = fsst->set(NULL, kv_value1, 64, 0x2/* flags */);
    utest_printf("Set1: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-) Key length exceeds max\n");
    err = fsst->set(NULL, kv_value1, KVStore::MAX_KEY_SIZE+10, 0x2/* flags */);
    utest_printf("Set1: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-) Buffer is NULL Size > 0\n");
    err = fsst->set(kv_key1, NULL, 64, 0x2/* flags */);
    utest_printf("Set1: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(+) Buffer is valid Size = 0\n");
    err = fsst->set(kv_key1, kv_value1, 0, 0x2/* flags */);
    utest_printf("Set1: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    utest_printf("(+) Set Key 1 twice, get second value\n");
    err = fsst->set(kv_key1, kv_value1, 64, 0x2/* flags */);
    utest_printf("Set1: %d\n", err);
    err = fsst->set(kv_key1, kv_value2, strlen(kv_value2), 0x2/* flags */);
    utest_printf("Set1: %d\n", err);
    memset(kv_buf, 0 ,64);
    err = fsst->get(kv_key1, kv_buf, 64, &actual_size, 0/*offset*/);
    utest_printf("Get1 - err: %d, act_size: %d\n", err, (int)actual_size);
    utest_printf("Get1 - data: %s\n", kv_buf);
    err = fsst->get_info(kv_key1, &kv_info);
    utest_printf("Get Info 1 - err: %d, flags: %d, size: %d\n", err, (int)kv_info.flags, (int)kv_info.size);
    TEST_ASSERT_EQUAL(0, err);

    utest_printf("(-) Get Key Null\n");
    memset(kv_buf, 0 ,64);
    err = fsst->get(NULL, kv_buf, 64, &actual_size, 0/*offset*/);
    utest_printf("Get1 - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(+) Get Key buffer null, size 0\n");
    memset(kv_buf, 0 ,64);
    err = fsst->get(kv_key1, NULL, 0, &actual_size, 0/*offset*/);
    utest_printf("Get1 - err: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    utest_printf("(-) Get Key buffer null, size > 0\n");
    memset(kv_buf, 0 ,64);
    err = fsst->get(kv_key1, NULL, 64, &actual_size, 0/*offset*/);
    utest_printf("Get1 - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(+) Get Key buffer smaller than actual size\n");
    memset(kv_buf, 0 ,64);
    err = fsst->get(kv_key1, kv_buf, 8, &actual_size, 0/*offset*/);
    utest_printf("Get1 - err: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    utest_printf("(-) Get Key Offset larger than actual size\n");
    memset(kv_buf, 0 ,64);
    err = fsst->get(kv_key1, kv_buf, 8, &actual_size, 128/*offset*/);
    utest_printf("Get1 - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-) Remove NULL key\n");
    err = fsst->remove(NULL);
    utest_printf("Remove - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-) Remove NON Existing key\n");
    err = fsst->remove("key4");
    utest_printf("Remove - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-) Get Info Key Null\n");
    err = fsst->get_info(NULL, &kv_info);
    utest_printf("Get1 - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-) Get Info - Info Null\n");
    err = fsst->get_info(kv_key1, NULL);
    utest_printf("Get1 - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-) Get Info - Non existing key\n");
    err = fsst->get_info("key4", &kv_info);
    utest_printf("Get1 - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-)It Open - NULL it \n");
    err = fsst->iterator_open(NULL, NULL);
    utest_printf("It Open - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-)It Next - key size 0 \n");
    err = fsst->iterator_open(&kv_it, NULL);
    err = fsst->iterator_next(kv_it, kv_name, 0);
    utest_printf("It Next - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);
    err = fsst->iterator_close(kv_it);

    utest_printf("(+) It Next on empty folder\n");
    err = fsst->reset();
    utest_printf("Reset Status - err: %d\n", err);
    err = fsst->iterator_open(&kv_it, NULL);
    err = fsst->iterator_next(kv_it, kv_name, 16);
    TEST_ASSERT_NOT_EQUAL(0, err);
    utest_printf("It Next - err: %d\n", err);
    err = fsst->iterator_close(kv_it);

    utest_printf("(+) It Next on 1 file folder\n");
    err = fsst->set(kv_key1, kv_value1, 64, 0x2/* flags */);
    utest_printf("Set1: %d\n", err);
    err = fsst->iterator_open(&kv_it, NULL);
    err = fsst->iterator_next(kv_it, kv_name, 16);
    utest_printf("It Next - err: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);
    err = fsst->iterator_next(kv_it, kv_name, 16);
    utest_printf("It Next - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);
    err = fsst->iterator_close(kv_it);


    utest_printf("(+) It Close after Open\n");
    err = fsst->iterator_open(&kv_it, NULL);
    err = fsst->iterator_close(kv_it);
    utest_printf("It Close - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    KVStore::set_handle_t handle;
    err = fsst->set_start(&handle, "key1", 64, 0x2);

    utest_printf("(-) set_start handle NULL\n");
    err = fsst->set_start(NULL, "key1", 64, 0x2);
    utest_printf("Set Start - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-) set_start key NULL\n");
    err = fsst->set_start(&handle, NULL, 64, 0x2);
    utest_printf("Set Start - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(+) set_start final size 0\n");
    err = fsst->set_start(&handle, "key1", 0, 0x2);
    utest_printf("Set Start - err: %d\n", err);
    err = fsst->set_finalize(handle);
    utest_printf("Set Finalize - err: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    utest_printf("(-) set_add handle NULL\n");
    err = fsst->set_add_data(NULL, "setvalue1", 10);
    utest_printf("Set  Add - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);

    utest_printf("(-) set_add value NULL\n");
    err = fsst->set_start(&handle, "key1", 0, 0x2);
    err = fsst->set_add_data(handle, NULL, 10);
    utest_printf("Set  Add - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);
    err = fsst->set_finalize(handle);

    utest_printf("(+) set_add size 0\n");
    utest_printf("OFR_DBG Calling Set_Start\n");
    err = fsst->set_start(&handle, "key1", 10, 0x2);
    utest_printf("Set Start - Size 10, err: %d\n", err);
    utest_printf("OFR_DBG Calling Set_Add_Data\n");
    err = fsst->set_add_data(handle, "abcde12345", 10);
    utest_printf("Set  Add 10 - err: %d\n", err);
    memset(kv_buf, 0 ,64);
    utest_printf("OFR_DBG Calling Get\n");
    err = fsst->get(kv_key1, kv_buf, 10, &actual_size, 0/*offset*/);
    utest_printf("Get1 Before Finalize - err: %d, data: %s\n", err, kv_buf);
    err = fsst->set_add_data(handle, "abcde12345", 0);
    utest_printf("Set  Add 0 - err: %d\n", err);
    err = fsst->set_finalize(handle);
    utest_printf("Set Finalize - err: %d\n", err);

    utest_printf("(+) Get Key1 After Finalize\n");
    memset(kv_buf, 0 ,64);
    err = fsst->get(kv_key1, kv_buf, 10, &actual_size, 0/*offset*/);
    utest_printf("Get1 - err: %d, data: %s\n", err, kv_buf);


    utest_printf("(-) set_add exceed final size\n");
    err = fsst->set_start(&handle, "key1", 10, 0x2);
    utest_printf("Set Start - Size 10, err: %d\n", err);
    err = fsst->set_add_data(handle, "abcde12345", 5);
    utest_printf("Set  Add 5 - err: %d\n", err);
    err = fsst->set_add_data(handle, "abcde12345", 10);
    utest_printf("Set  Add 10 - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);
    err = fsst->set_add_data(handle, "abcde12345", 5);
    utest_printf("Set  Add 5 - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);
    err = fsst->set_finalize(handle);
    utest_printf("Set Finalize - err: %d\n", err);

    utest_printf("(-) set_add size doesnt match final size\n");
    err = fsst->set_start(&handle, "key1", 10, 0x2);
    utest_printf("Set Start - Size 10, err: %d\n", err);
    err = fsst->set_add_data(handle, "abcde12345", 5);
    utest_printf("Set  Add 5 - err: %d\n", err);
    err = fsst->set_add_data(handle, "abcde12345", 3);
    utest_printf("Set  Add 3 - err: %d\n", err);
    err = fsst->set_finalize(handle);
    utest_printf("Set Finalize - err: %d\n", err);
    TEST_ASSERT_NOT_EQUAL(0, err);



    err = fsst->deinit();
    utest_printf("deinit fsst: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    err = bd.deinit();
    TEST_ASSERT_EQUAL(0, err);
}

void test_file_system_store_multi_threads()
{
	char kv_buf[64] = {0};

	utest_printf("\nTest Multi Threaded FileSystemStore Set Starts..\n");

    int err = bd.init();
    TEST_ASSERT_EQUAL(0, err);

    LittleFileSystem fs("lfs", &bd);
    //FATFileSystem fs("fatfs", &bd);

    err = fs.mount(&bd);

    if (err) {
    	err = fs.reformat(&bd);
    	TEST_ASSERT_EQUAL(0, err);
    }

    FileSystemStore *fsst = new FileSystemStore(&fs);

    err = fsst->init();
    utest_printf("init fsst: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    err = fsst->reset();
    utest_printf("Reset FSST, err: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    thread_data_t thread_data[3];

    /* Thread Access Test Starts */
    rtos::Thread set_thread[3];

    osStatus threadStatus;
    int i_ind = 0;

    for (i_ind=0; i_ind < 3; i_ind++) {
        thread_data[i_ind].fsst = fsst;
        thread_data[i_ind].thread_num = i_ind+1;
        threadStatus = set_thread[i_ind].start(test_set_thread_job, (void *)&(thread_data[i_ind]));
    }

    for (i_ind=0; i_ind < 3; i_ind++) {
        set_thread[i_ind].join();
    }


    char kv_value[12] = {"valuevalue"};
    char kv_key[6] = {"key"};
    char thread_str[3] = {0};

    size_t actual_size = 0;

    for (i_ind=1; i_ind < 4; i_ind++) {
    	memset(kv_buf, 0 ,64);
    	strcat(&kv_value[10],itoa(i_ind, thread_str, 10));
    	strcat(&kv_key[3],itoa(i_ind, thread_str, 10));
    	err = fsst->get(kv_key, kv_buf, 10, &actual_size, 0);
    	TEST_ASSERT_EQUAL(0, err);
    	utest_printf("Key File: %s, value: %s\n",kv_key, kv_buf);
    	TEST_ASSERT_EQUAL(0, strcmp(kv_value,kv_buf));
    }

	err = fsst->reset();
	TEST_ASSERT_EQUAL(0, err);

    err = fsst->deinit();
    utest_printf("deinit fsst: %d\n", err);
    TEST_ASSERT_EQUAL(0, err);

    err = bd.deinit();
    TEST_ASSERT_EQUAL(0, err);
}

// Test setup
utest::v1::status_t test_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(60, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("Testing functionality APIs unit test", test_file_system_store_functionality_unit_test),
    Case("Testing Edge Cases", test_file_system_store_edge_cases),
    Case("Testing Multi Threads Set", test_file_system_store_multi_threads)
};

Specification specification(test_setup, cases);


int main()
{
    mbed_trace_init();
    utest_printf("MAIN STARTS\n");
    return !Harness::run(specification);
}

