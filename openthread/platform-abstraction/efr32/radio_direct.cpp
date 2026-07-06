/*******************************************************************************
 * @file
 * @brief EFR32 platform implementation of Thread Direct radio APIs.
 *
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

#include "radio_direct.h"

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

#include <string.h>
#include <openthread/link.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/thread_direct.h>
#include "common/code_utils.hpp"
#include "common/debug.hpp"
#include "utils/code_utils.h"
#include "utils/mac_frame.h"

#include "platform-efr32.h"
#include "radio_instance.h"
#include "radio_security.h"

// Per-instance SLW (Scheduled Listen Window) state, mirroring the CSL pattern in radio_csl.cpp.
typedef struct
{
    uint32_t sampleTime;  // Next expected SLW frame arrival time in us.
    uint16_t period;      // SLW period in 160 us slots (0 = disabled).
    int16_t  ramOffsetUs; // RAM offset [-1024, 1023] us.
} slw_state_t;

static slw_state_t sSlwState[RADIO_INTERFACE_COUNT];

static slw_state_t *getSlwState(otInstance *aInstance)
{
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    instanceIndex_t index = sli_ot_radio_instance_get_index(aInstance);
    OT_ASSERT(index < RADIO_INTERFACE_COUNT);
    return &sSlwState[index];
#else
    OT_UNUSED_VARIABLE(aInstance);
    return &sSlwState[0];
#endif
}

void otPlatRadioSetWakeKey(otInstance *aInstance, uint8_t aKeyIndex, const otMacKeyMaterial *aWakeKey)
{
    sli_ot_radio_security_set_wake_key(aInstance, aKeyIndex, aWakeKey);
}

// Called from the time-critical RAIL data-request callback within the
// 192 us IEEE 802.15.4 ACK turnaround window.
#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE
uint8_t sli_ot_radio_direct_generate_enh_ack_ie_data(otInstance   *aInstance,
                                                     otRadioFrame *aReceivedFrame,
                                                     uint8_t      *aIeData,
                                                     uint8_t       aAvailable)
{
    OT_UNUSED_VARIABLE(aInstance);
    return otMacFrameGenerateThreadDirectEnhAckIe(aReceivedFrame, aIeData, aAvailable);
}

#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE

otError otPlatRadioSetThreadDirectSlwSchedule(otInstance *aInstance, uint16_t aSlwPeriod, int16_t aRamOffsetUs)
{
    slw_state_t *state = getSlwState(aInstance);
    state->period      = aSlwPeriod;
    state->ramOffsetUs = aRamOffsetUs;
    return OT_ERROR_NONE;
}

void otPlatRadioUpdateThreadDirectSlwSampleTime(otInstance *aInstance, uint32_t aSlwSampleTime)
{
    slw_state_t *state = getSlwState(aInstance);
    state->sampleTime  = aSlwSampleTime;
}

bool sli_ot_radio_direct_slw_is_present(otInstance *aInstance)
{
    return getSlwState(aInstance)->period != 0;
}

uint16_t sli_ot_radio_direct_slw_get_period(otInstance *aInstance)
{
    return getSlwState(aInstance)->period;
}

uint32_t sli_ot_radio_direct_slw_get_sample_time(otInstance *aInstance)
{
    return getSlwState(aInstance)->sampleTime;
}

int16_t sli_ot_radio_direct_slw_get_ram_offset(otInstance *aInstance)
{
    return getSlwState(aInstance)->ramOffsetUs;
}

uint16_t sli_ot_radio_direct_slw_get_phase(otInstance *aInstance, uint32_t aShrTxTime)
{
    slw_state_t *state      = getSlwState(aInstance);
    uint32_t     periodInUs = static_cast<uint32_t>(state->period) * OT_US_PER_TEN_SYMBOLS;
    uint32_t     adjustedUs = state->sampleTime - aShrTxTime - static_cast<uint32_t>(state->ramOffsetUs);

    return static_cast<uint16_t>((adjustedUs % periodInUs) / OT_US_PER_TEN_SYMBOLS);
}

// Delegates to CSL accuracy functions; both use the same oscillator.
OT_TOOL_WEAK uint8_t otPlatRadioGetThreadDirectSlwAccuracy(otInstance *aInstance)
{
    return otPlatRadioGetCslAccuracy(aInstance);
}

OT_TOOL_WEAK uint8_t otPlatRadioGetThreadDirectSlwUncertainty(otInstance *aInstance)
{
    return otPlatRadioGetCslUncertainty(aInstance);
}

OT_TOOL_WEAK otError otPlatRadioGetThreadDirectRamParams(otInstance *aInstance, otThreadDirectRamParams *aParams)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aParams);
    return OT_ERROR_NOT_IMPLEMENTED;
}

#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
