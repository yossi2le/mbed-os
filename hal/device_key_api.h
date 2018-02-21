
/** \addtogroup hal */
/** @{*/
/* mbed Microcontroller Library
 * Copyright (c) 2016 ARM Limited
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

/**
 *  DEVICE_KEY - Root of Trust when provided by the platform is a secret and unique per-device value 128 or 256 bits long that will be used to generate additional keys.
 *  DEVICE_KEY API to be used to generate keys is presented by DEVICE_KEY driver. If DEVICE_KEY is not implemented in a platform, DEVICE_KEY driver will generate it from random.
 *
 */

#ifndef MBED_DEVICE_KEY_API_H
#define MBED_DEVICE_KEY_API_H

#include "stddef.h"
#include "stdint.h"
#include "fsl_common.h"


#if DEVICE_DEVKEY
#define DEVICE_KEY_LEN 16   /* DEVICE_KEY key length in bytes */


#ifdef __cplusplus
extern "C" {
#endif
/**
 * \defgroup hal_device_key DEVICE_KEY hal functions
 * @{
 */

/** Get data for DeviceKey from the platform
*
* @param output The pointer to an output array
* @param length in: The size of output data buffer out: Size of actual data written
* @return 0 success, -1 fail
*/
int device_key_get_value(uint32_t *output, size_t *length);

/** Return the size of the device key in bytes
 *
 * @retrun the device key length
 */
int device_key_get_size_in_bytes();

/**@}*/

#ifdef __cplusplus
}
#endif

#endif

#endif

/** @}*/
