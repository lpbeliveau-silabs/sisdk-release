/*******************************************************************************
 * @file
 * @brief Core application logic for the Thread Direct Wake Listener demo.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include <assert.h>
#include <openthread-core-config.h>
#include <openthread/config.h>

#include <openthread/cli.h>
#include <openthread/diag.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/thread_direct.h>

#include "app.h"
#include "openthread-system.h"

#include "sl_component_catalog.h"

void sleepyInit(void);
void setNetworkConfiguration(void);
#ifdef SL_CATALOG_KERNEL_PRESENT
void sl_ot_rtos_application_tick(void);
#else
void applicationTick(void);
#endif

extern void otAppCliInit(otInstance *aInstance);

static otInstance *sInstance = NULL;

#if OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_APP
void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aFormat);

    va_list ap;
    va_start(ap, aFormat);
    otCliPlatLogv(aLogLevel, aLogRegion, aFormat, ap);
    va_end(ap);
}
#endif

void sl_ot_create_instance(void)
{
    sInstance = otInstanceInitSingle();
    assert(sInstance);
}

void sl_ot_cli_init(void)
{
    otAppCliInit(sInstance);
}

/******************************************************************************
 * Application Init.
 *****************************************************************************/
void app_init(void)
{
    // Configure ext address, SLW schedule, and event callback.
    sleepyInit();

    // Load dataset so the network key is available for Wake Frame decryption.
    // Thread is not started; the WL does not attach to the network.
    setNetworkConfiguration();
    assert(otIp6SetEnabled(sInstance, true) == OT_ERROR_NONE);

    if (otThreadDirectWakeListenerEnable(sInstance, true) == OT_ERROR_NONE)
    {
        otCliOutputFormat("Listening for TD Wake Commands on wake channel %u...\r\n",
                          OPENTHREAD_CONFIG_THREAD_DIRECT_DEFAULT_WAKE_CHANNEL);
    }
    else
    {
        assert(false);
    }
}

/******************************************************************************
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
    otTaskletsProcess(sInstance);
    otSysProcessDrivers(sInstance);

#ifdef SL_CATALOG_KERNEL_PRESENT
    sl_ot_rtos_application_tick();
#else
    applicationTick();
#endif
}

/******************************************************************************
 * Application Exit.
 *****************************************************************************/
void app_exit(void)
{
    otInstanceFinalize(sInstance);
}
