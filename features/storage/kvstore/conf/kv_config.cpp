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

#include "kv_config.h"

int _storage_config_TDB_INTERNAL()
{
    return -1;
}

int _storage_config_TDB_EXTERNAL()
{
    return -1;
}

int _storage_config_TDB_EXTERNAL_NO_RBP()
{
    return -1;
}

int _storage_config_FILESYSTEM()
{
    return -1;
}

int _storage_config_FILESYSTEM_NO_RBP()
{
    return -1;
}

int storage_configuration()
{

    //return _STORAGE_CONFIG(MBED_CONF_STORAGE_STORAGE_TYPE);
    return _storage_config_FILESYSTEM_NO_RBP();
}
