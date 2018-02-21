/*
 *  Return the values stored in registers UIDH, UIDMH, UIDML, UIDL. This value is predictable therefore not secure, but used as example
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
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
 */

/*
 * Reference: "K64 Sub-Family Reference Manual, Rev. 2", chapter 13.2.22
 */

#if defined(DEVICE_DEVKEY)

#include <stdlib.h>
#include "cmsis.h"
#include "fsl_common.h"
#include "fsl_clock.h"
#include "device_key_api.h"


int device_key_get_value(uint32_t *output, size_t *length)
{
    unsigned int i;

    if (*length < DEVICE_KEY_LEN)
        return -1;

    for (i=0;i<*length;i++){
        *(((char *)output)+i) = 0;
    }

    *output++ = SIM->UIDH;
    *output++ = SIM->UIDML;
    *output++ = SIM->UIDMH;
    *output++ = SIM->UIDL;
    *length=sizeof(*output)*4;

    return 0;
}


int device_key_get_size_in_bytes()
{
	return DEVICE_KEY_LEN;
}

#endif
