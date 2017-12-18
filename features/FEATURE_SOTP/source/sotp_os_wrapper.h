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



#ifndef __SOTP_OS_WRAPPER_H
#define __SOTP_OS_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOTP_OS_OK              0
#define SOTP_OS_RTOS_ERR        1
#define SOTP_OS_INV_ARG_ERR     2
#define SOTP_OS_NO_MEM_ERR      3

#define SOTP_MIN(a,b)            ((a) < (b) ? (a) : (b))
#define SOTP_MAX(a,b)            ((a) > (b) ? (a) : (b))

typedef uintptr_t sotp_mutex_t;
typedef uintptr_t sotp_shared_lock_t;

#if SOTP_THREAD_SAFE

uint32_t sotp_atomic_increment(uint32_t* value, uint32_t increment);

uint32_t sotp_atomic_decrement(uint32_t* value, uint32_t increment);

int sotp_delay(uint32_t millisec);

sotp_mutex_t sotp_mutex_create(void);

int sotp_mutex_wait(sotp_mutex_t mutex, uint32_t millisec);

int sotp_mutex_release(sotp_mutex_t mutex);

int sotp_mutex_destroy(sotp_mutex_t mutex);

#endif

sotp_shared_lock_t sotp_sh_lock_create(void);

int sotp_sh_lock_destroy(sotp_shared_lock_t sh_lock);

int sotp_sh_lock_shared_lock(sotp_shared_lock_t sh_lock);

int sotp_sh_lock_shared_release(sotp_shared_lock_t sh_lock);

int sotp_sh_lock_exclusive_lock(sotp_shared_lock_t sh_lock);

int sotp_sh_lock_exclusive_release(sotp_shared_lock_t sh_lock);

int sotp_sh_lock_promote(sotp_shared_lock_t sh_lock);


#ifdef __cplusplus
}
#endif

#endif
