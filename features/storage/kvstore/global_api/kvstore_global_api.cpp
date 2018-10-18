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
#include "kvstore_global_api.h"

#include "kv_map.h"
#include "KVStore.h"

// iterator handle
struct _opaque_kv_key_iterator{
    KVStore *kvstore_intance;
    KVStore::iterator_t *iterator_handle;
};

int kv_set(const char *full_name_key, const void *buffer, size_t size, uint32_t create_flags)
{
    int ret = kv_init();
    if (KVSTORE_SUCCESS != ret) {
        return ret;
    }

    KVStore * kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_name_key, &kv_instance, key);
    return kv_instance->set(key, buffer, size, create_flags);
}

int kv_get(const char *full_name_key, void *buffer, size_t buffer_size, size_t *actual_size)
{
    int ret = kv_init();
    if (KVSTORE_SUCCESS != ret) {
        return ret;
    }

    KVStore * kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_name_key, &kv_instance, key);
    return kv_instance->get(key, buffer, buffer_size, actual_size);
}

int kv_get_info(const char *full_name_key, kv_info_t *info)
{
    int ret = kv_init();
    if (KVSTORE_SUCCESS != ret) {
        return ret;
    }

    KVStore * kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_name_key, &kv_instance, key);
    KVStore::info_t inner_info;
    ret =  kv_instance->get_info(key, &inner_info);
    if (KVSTORE_SUCCESS != ret) {
        return ret;
    }
    info->flags = inner_info.flags;
    info->size =  inner_info.size;
    return ret;
}

int kv_remove(const char *full_name_key)
{
    int ret = kv_init();
    if (KVSTORE_SUCCESS != ret) {
        return ret;
    }

    KVStore * kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_name_key, &kv_instance, key);
    return kv_instance->remove(key);
}

int kv_iterator_open(kv_iterator_t *it, const char *full_prefix)
{
    int ret = kv_init();
    if (KVSTORE_SUCCESS != ret) {
        return ret;
    }

    (*it) = new _opaque_kv_key_iterator;
    if (*it == NULL) {
        return KVSTORE_OS_ERROR;
    }

    KVStore * kv_instance = NULL;
    char key[KV_MAX_KEY_LENGTH];
    kv_lookup(full_prefix, &kv_instance, key);
    (*it)->kvstore_intance = kv_instance;

    KVStore::iterator_t * inner_it = NULL;
    ret = kv_instance->iterator_open(inner_it, key);
    if (KVSTORE_SUCCESS != ret) {
        return ret;
    }

    (*it)->iterator_handle = inner_it;

    return ret;

}

int kv_iterator_next(kv_iterator_t it, char *key, size_t key_size)
{
    int ret = kv_init();
    if (KVSTORE_SUCCESS != ret) {
        return ret;
    }

    return it->kvstore_intance->iterator_next(*it->iterator_handle, key, key_size);
}

int kv_iterator_close(kv_iterator_t it)
{
    int ret = kv_init();
    if (KVSTORE_SUCCESS != ret) {
        return ret;
    }

    ret = it->kvstore_intance->iterator_close(*it->iterator_handle);

    delete it;

    return ret;
}

