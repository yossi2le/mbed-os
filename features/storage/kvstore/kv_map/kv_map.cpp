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

#include "KVStore.h"
#include "kv_map.h"
#include "kv_config.h"
#include <stdlib.h>
#include "platform/PlatformMutex.h"
#include "platform/SingletonPtr.h"
#include "string.h"
#include "mbed_error.h"

using namespace mbed;

#define  MAX_ATTACHED_KVS 16

typedef struct {
    char *partition_name;
    KVStore *kvstore_instance;
} kv_map_entry_t;

// Attachment table
kv_map_entry_t kv_map_table[MAX_ATTACHED_KVS];
int kv_num_attached_kvs;

static int is_initialized = 0;
static SingletonPtr<PlatformMutex> mutex;

int kv_init()
{
    int ret = MBED_SUCCESS;

    mutex->lock();

    if (is_initialized) {
        goto exit;
    }

    kv_num_attached_kvs = 0;
    memset(&kv_map_table, 0, sizeof(kv_map_table));

    is_initialized = 1;

exit:
    mutex->unlock();
    return ret;
}

int kv_attach(const char *partition_name, KVStore *kv_instance)
{
    int ret = MBED_SUCCESS;

    mutex->lock();

    if (!is_initialized) {
        ret = MBED_ERROR_NOT_READY;
        goto exit;
    }

    if (kv_num_attached_kvs >= MAX_ATTACHED_KVS) {
        ret =  MBED_ERROR_OUT_OF_MEMORY;
        goto exit;
    }

    kv_map_table[kv_num_attached_kvs].partition_name = strdup(partition_name);
    kv_map_table[kv_num_attached_kvs].kvstore_instance = kv_instance;
    kv_num_attached_kvs++;

exit:
    mutex->unlock();
    return ret;
}

int kv_detach(const char *partition_name)
{
    int ret = MBED_SUCCESS;

    mutex->lock();

    if (!is_initialized) {
        ret = MBED_ERROR_NOT_READY;
        goto exit;
    }

    ret = MBED_ERROR_ITEM_NOT_FOUND;
    for (int i = 0; i < kv_num_attached_kvs; i++ ) {

        if (strcmp(partition_name, kv_map_table[i].partition_name) != 0) {
            continue;
        }

        free(kv_map_table[i].partition_name);
        kv_map_table[i].kvstore_instance->deinit();

        memcpy(&kv_map_table[i], &kv_map_table[i + 1], sizeof(kv_map_entry_t) * (MAX_ATTACHED_KVS - i - 1));
        kv_map_table[MAX_ATTACHED_KVS - 1].partition_name = NULL;
        kv_map_table[MAX_ATTACHED_KVS - 1].kvstore_instance = NULL;
        kv_num_attached_kvs--;
        ret = MBED_SUCCESS;
        break;
    }

exit:
    mutex->unlock();
    return ret;
}

int kv_deinit()
{
    int ret = MBED_SUCCESS;

    mutex->lock();

    if (!is_initialized) {
        ret = MBED_ERROR_NOT_READY;
        goto exit;
    }

    for (int i = 0; i < kv_num_attached_kvs; i++ ) {

        if (kv_map_table[i].kvstore_instance == NULL) {
            goto exit;
        }

        free(kv_map_table[i].partition_name);
        if (kv_map_table[i].kvstore_instance != NULL) {
            kv_map_table[i].kvstore_instance->deinit();
        }
        kv_map_table[i].partition_name = NULL;
        kv_map_table[i].kvstore_instance = NULL;
    }

exit:
    kv_num_attached_kvs = 0;
    mutex->unlock();
    return ret;
}

//return -1 if delimiter is not found
int kv_get_delimiter_index(const char *str, char delimiter)
{
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++ ) {
        if (str[i] != delimiter) {
            continue;
        }

        return i;
    }

    return -1;
}

// Full name lookup and then break it into KVStore instance and key
int kv_lookup(const char *full_name, KVStore **kv_instance, size_t *key_index)
{
    int ret = MBED_SUCCESS;
    int delimiter_index;
    int i;
    mutex->lock();
    const char *temp_str = full_name;

    if (!is_initialized) {
        ret = MBED_ERROR_NOT_READY;
        goto exit;
    }

    *key_index = 0;
    if (*temp_str == '/') {
        temp_str++;
        (*key_index)++;
    }

    delimiter_index = kv_get_delimiter_index(temp_str, '/');
    if (delimiter_index == -1) { //delimiter not found
        *kv_instance = kv_map_table[0].kvstore_instance;
        goto exit;
    }

    for (i = 0; i < kv_num_attached_kvs; i++ ) {

        if (memcmp(temp_str, kv_map_table[i].partition_name, delimiter_index) != 0) {
            continue;
        }

        *kv_instance = kv_map_table[i].kvstore_instance;
        break;
    }

    if (i == kv_num_attached_kvs) {
        ret = MBED_ERROR_ITEM_NOT_FOUND;
        goto exit;
    }

exit:
    if (ret == MBED_SUCCESS) {
        //if success extarct the key
        *key_index = *key_index + delimiter_index + 1;
    }

    mutex->unlock();
    return ret;
}
