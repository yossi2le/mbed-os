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



// ----------------------------------------------------------- Includes -----------------------------------------------------------

#include "mbed.h"

#include "sotp_os_wrapper.h"
#include "cmsis_os2.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --------------------------------------------------------- Definitions ----------------------------------------------------------

#define TRACE_GROUP                     "sotp"

#define PR_ERR printf

#define MEDITATE_TIME_MS 100

typedef struct {
    osMutexId_t               os_mutex_id;
    osMutexAttr_t             os_mutex;
    mbed_rtos_storage_mutex_t os_mutex_storage;
} mutex_priv_t;

typedef struct {
    uint32_t      ctr;
    sotp_mutex_t  mutex;
} shared_lock_priv_t;

#ifdef __cplusplus
extern "C" {
#endif


// -------------------------------------------------- Functions Implementation ----------------------------------------------------

#if SOTP_THREAD_SAFE

uint32_t sotp_atomic_increment(uint32_t* value, uint32_t increment)
{
    return core_util_atomic_incr_u32(value, increment);
}

uint32_t sotp_atomic_decrement(uint32_t* value, uint32_t increment)
{
    return core_util_atomic_decr_u32(value, increment);
}

int sotp_delay(uint32_t millisec)
{
    osStatus st;

    st = osDelay(millisec);
    if (st != osOK) {
        return -SOTP_OS_RTOS_ERR;
    }

    return SOTP_OS_OK;
}

sotp_mutex_t sotp_mutex_create(void)
{

    mutex_priv_t *mutex_priv;
    mutex_priv = (mutex_priv_t *) malloc(sizeof(mutex_priv_t));

    if (!mutex_priv) {
        PR_ERR("sotp_mutex_create: Out of memory\n");
        return (sotp_mutex_t) 0;
    }

    mutex_priv->os_mutex.name = NULL;
    mutex_priv->os_mutex.attr_bits = osMutexRecursive | osMutexRobust;
    mutex_priv->os_mutex.cb_mem = &mutex_priv->os_mutex_storage;
    mutex_priv->os_mutex.cb_size = sizeof(mutex_priv->os_mutex_storage);
    memset(&mutex_priv->os_mutex_storage, 0, sizeof(mutex_priv->os_mutex_storage));

    mutex_priv->os_mutex_id = osMutexNew(&mutex_priv->os_mutex);
    if (!mutex_priv->os_mutex_id) {
        PR_ERR("sotp_mutex_create: osMutexNew failed\n");
        free(mutex_priv);
        return (sotp_mutex_t) 0;
    }

    return (sotp_mutex_t) mutex_priv;
}


int sotp_mutex_wait(sotp_mutex_t mutex, uint32_t millisec)
{
    mutex_priv_t *mutex_priv = (mutex_priv_t *) mutex;
    osStatus_t st;

    if (!mutex_priv) {
        PR_ERR("sotp_mutex_wait: NULL parameter\n");
        return -SOTP_OS_INV_ARG_ERR;
    }

    st = osMutexAcquire(mutex_priv->os_mutex_id, millisec);
    if (st != osOK) {
        return -SOTP_OS_RTOS_ERR;
    }

    return SOTP_OS_OK;
}

int sotp_mutex_release(sotp_mutex_t mutex)
{
    mutex_priv_t *mutex_priv = (mutex_priv_t *) mutex;
    osStatus_t st;

    if (!mutex_priv) {
        PR_ERR("sotp_mutex_release: NULL parameter\n");
        return -SOTP_OS_INV_ARG_ERR;
    }

    st = osMutexRelease(mutex_priv->os_mutex_id);
    if (st != osOK) {
        return -SOTP_OS_RTOS_ERR;
    }

    return SOTP_OS_OK;
}

int sotp_mutex_destroy(sotp_mutex_t mutex)
{
    mutex_priv_t *mutex_priv = (mutex_priv_t *) mutex;
    osStatus_t st;

    if (!mutex_priv) {
        PR_ERR("sotp_mutex_release: NULL parameter\n");
        return -SOTP_OS_INV_ARG_ERR;
    }

    st = osMutexDelete(mutex_priv->os_mutex_id);
    if (st != osOK) {
        return -SOTP_OS_RTOS_ERR;
    }

    free(mutex_priv);
    return SOTP_OS_OK;
}
#endif

// Create a shared lock.
// Parameters :
// sh_lock    - [OUT]  lock handle.
// Return     : 0 or error code.
sotp_shared_lock_t sotp_sh_lock_create(void)
{
#if SOTP_THREAD_SAFE
    shared_lock_priv_t *lock_priv;
    lock_priv = (shared_lock_priv_t *) malloc(sizeof(shared_lock_priv_t));

    if (!lock_priv) {
        PR_ERR("sotp_sh_lock_create: Out of memory\n");
        return (sotp_shared_lock_t) 0;
    }

    lock_priv->ctr = 0;

    if (!(lock_priv->mutex = sotp_mutex_create())) {
        PR_ERR("sotp_sh_lock_shared_lock: mutex create error\n");
        free(lock_priv);
        return (sotp_shared_lock_t) NULL;
    }

    return (sotp_shared_lock_t) lock_priv;
#else
    // Just return a non zero result (irrelevant for all other functions)
    return (sotp_shared_lock_t) 1;
#endif
}

// Destroy a shared lock.
// Parameters :
// sh_lock    - [OUT]  lock handle.
// Return     : 0 or error code.
int sotp_sh_lock_destroy(sotp_shared_lock_t sh_lock)
{
#if SOTP_THREAD_SAFE
    shared_lock_priv_t *lock_priv = (shared_lock_priv_t *) sh_lock;

    if (!sh_lock) {
        PR_ERR("sotp_sh_lock_destroy: NULL parameter\n");
        return -SOTP_OS_INV_ARG_ERR;
    }

    if (sotp_mutex_destroy(lock_priv->mutex)) {
        PR_ERR("sotp_sh_lock_destroy: mutex delete error\n");
        return -SOTP_OS_RTOS_ERR;
    }

    free(lock_priv);
#endif
    return SOTP_OS_OK;
}

// Lock a shared-lock in a shared manner.
// Parameters :
// sh_lock    - [OUT]  lock handle.
// Return     : 0 or error code.
int sotp_sh_lock_shared_lock(sotp_shared_lock_t sh_lock)
{
#if SOTP_THREAD_SAFE
    shared_lock_priv_t *lock_priv = (shared_lock_priv_t *) sh_lock;

    if (!sh_lock) {
        PR_ERR("sotp_sh_lock_shared_lock: NULL parameter\n");
        return -SOTP_OS_INV_ARG_ERR;
    }

    if (sotp_mutex_wait(lock_priv->mutex, osWaitForever)) {
        PR_ERR("sotp_sh_lock_shared_lock: OS error\n");
        return -SOTP_OS_RTOS_ERR;
    }

    sotp_atomic_increment(&lock_priv->ctr, 1);

    if (sotp_mutex_release(lock_priv->mutex)) {
        PR_ERR("sotp_sh_lock_shared_lock: OS error\n");
        return -SOTP_OS_RTOS_ERR;
    }
#endif
    return SOTP_OS_OK;
}

// Release a shared-lock in a shared manner.
// Parameters :
// sh_lock    - [OUT]  lock handle.
// Return     : 0 or error code.
int sotp_sh_lock_shared_release(sotp_shared_lock_t sh_lock)
{
#if SOTP_THREAD_SAFE
    shared_lock_priv_t *lock_priv = (shared_lock_priv_t *) sh_lock;
    int32_t val;

    if (!sh_lock) {
        PR_ERR("sotp_sh_lock_shared_release: NULL parameter\n");
        return -SOTP_OS_INV_ARG_ERR;
    }

    val = sotp_atomic_decrement(&lock_priv->ctr, 1);
    if (val < 0) {
        PR_ERR("sotp_sh_lock_shared_release: Misuse (released more than locked)\n");
        return -SOTP_OS_RTOS_ERR;
    }
#endif

    return SOTP_OS_OK;
}

// Lock a shared-lock in an exclusive manner.
// Parameters :
// sh_lock    - [OUT]  lock handle.
// Return     : 0 or error code.
int sotp_sh_lock_exclusive_lock(sotp_shared_lock_t sh_lock)
{
#if SOTP_THREAD_SAFE
    shared_lock_priv_t *lock_priv = (shared_lock_priv_t *) sh_lock;

    if (!sh_lock) {
        PR_ERR("sotp_sh_lock_exclusive_lock: NULL parameter\n");
        return -SOTP_OS_INV_ARG_ERR;
    }

    if (sotp_mutex_wait(lock_priv->mutex, osWaitForever)) {
        PR_ERR("sotp_sh_lock_exclusive_lock: OS error\n");
        return -SOTP_OS_RTOS_ERR;
    }

    while(lock_priv->ctr)
        sotp_delay(MEDITATE_TIME_MS);
#endif

    return SOTP_OS_OK;
}

// Release a shared-lock in an exclusive manner.
// Parameters :
// sh_lock    - [OUT]  lock handle.
// Return     : SOTP_SHL_SUCCESS on success. Error code otherwise.
int sotp_sh_lock_exclusive_release(sotp_shared_lock_t sh_lock)
{
#if SOTP_THREAD_SAFE
    shared_lock_priv_t *lock_priv = (shared_lock_priv_t *) sh_lock;

    if (!sh_lock) {
        PR_ERR("sotp_sh_lock_exclusive_release: NULL parameter\n");
        return -SOTP_OS_INV_ARG_ERR;
    }

    if (sotp_mutex_release(lock_priv->mutex)) {
        PR_ERR("sotp_sh_lock_exclusive_release: OS error\n");
        return -SOTP_OS_RTOS_ERR;
    }
#endif

    return SOTP_OS_OK;
}

// Promote a shared-lock from shared mode to exclusive mode.
// Parameters :
// sh_lock    - [OUT]  lock handle.
// Return     : SOTP_SHL_SUCCESS on success. Error code otherwise.
int sotp_sh_lock_promote(sotp_shared_lock_t sh_lock)
{
#if SOTP_THREAD_SAFE
    shared_lock_priv_t *lock_priv = (shared_lock_priv_t *) sh_lock;

    if (!sh_lock) {
        PR_ERR("sotp_sh_lock_promote: NULL parameter\n");
        return -SOTP_OS_INV_ARG_ERR;
    }

    if (sotp_mutex_wait(lock_priv->mutex, osWaitForever)) {
        PR_ERR("sotp_sh_lock_promote: OS error\n");
        return -SOTP_OS_RTOS_ERR;
    }

    while(lock_priv->ctr > 1)
        sotp_delay(MEDITATE_TIME_MS);

    if (lock_priv->ctr != 1) {
        PR_ERR("sotp_sh_lock_promote: Misuse (promoted when not locked)\n");
        return -SOTP_OS_RTOS_ERR;
    }

    sotp_atomic_decrement(&lock_priv->ctr, 1);
#endif

    return SOTP_OS_OK;
}

#ifdef __cplusplus
}
#endif

