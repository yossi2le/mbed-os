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



#ifndef __SOTP_INT_FLASH_WRAPPER_H
#define __SOTP_INT_FLASH_WRAPPER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOTP_BLANK_FLASH_VAL 0xFF

#ifndef SOTP_INT_FLASH_NUM_AREAS
#define SOTP_INT_FLASH_NUM_AREAS        2
#endif

typedef struct
{
    uint32_t address;
    size_t   size;
} sotp_area_data_t;

int sotp_int_flash_init(void);

int sotp_int_flash_deinit(void);

int sotp_int_flash_read(size_t size, uint32_t address, uint32_t *buffer);

int sotp_int_flash_erase(uint32_t address, size_t size);

int sotp_int_flash_write(size_t size, uint32_t address, const uint32_t *buffer);

int sotp_int_flash_get_area_info(uint8_t area, sotp_area_data_t *data);


#ifdef __cplusplus
}
#endif

#endif
