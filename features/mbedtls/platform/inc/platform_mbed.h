/**
 *  Copyright (C) 2006-2016, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

#include <stddef.h>

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#if defined(DEVICE_TRNG)

#define MBEDTLS_ENTROPY_HARDWARE_ALT

#else

#define MBEDTLS_PLATFORM_NV_SEED_ALT
#define MBEDTLS_ENTROPY_NV_SEED
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_NO_PLATFORM_ENTROPY

EXTERNC int platform_std_nv_seed_read( unsigned char *buf, size_t buf_len );
EXTERNC int platform_std_nv_seed_write( unsigned char *buf, size_t buf_len );

#define MBEDTLS_PLATFORM_STD_NV_SEED_READ   platform_std_nv_seed_read
#define MBEDTLS_PLATFORM_STD_NV_SEED_WRITE  platform_std_nv_seed_write

#endif //DEVICE_TRNG

#if defined(MBEDTLS_CONFIG_HW_SUPPORT)
#include "mbedtls_device.h"
#endif

#define MBEDTLS_ERR_PLATFORM_HW_FAILED       -0x0080

#undef EXTERNC
