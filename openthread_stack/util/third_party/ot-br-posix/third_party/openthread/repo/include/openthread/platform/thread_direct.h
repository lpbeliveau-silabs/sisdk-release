/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes the platform abstraction for Thread Direct link operations.
 */

#ifndef OPENTHREAD_PLATFORM_THREAD_DIRECT_H_
#define OPENTHREAD_PLATFORM_THREAD_DIRECT_H_

#include <stdint.h>

#include <openthread/error.h>
#include <openthread/instance.h>
#include <openthread/thread_direct.h>
#include <openthread/platform/radio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup plat-thread-direct
 *
 * @brief
 *   This module includes the platform abstraction for Thread Direct.
 *
 * @{
 *
 */

/**
 * Programs the platform with the local Scheduled Listen Window (SLW) schedule.
 *
 * @param[in] aInstance        The OpenThread instance.
 * @param[in] aSlwPeriod       SLW period in units of @p aSlotDurationUs
 *                             (0 = clear schedule / rx-on-when-idle).
 * @param[in] aSlotDurationUs  The advertised SLW Slot Duration, in microseconds.
 *                             Ignored when @p aSlwPeriod is 0.
 *
 * @retval OT_ERROR_NONE             Schedule updated.
 * @retval OT_ERROR_NOT_IMPLEMENTED  Feature is not implemented.
 */
otError otPlatRadioSetThreadDirectSlwSchedule(otInstance *aInstance, uint16_t aSlwPeriod, uint32_t aSlotDurationUs);

/**
 * Updates the next SLW sample time used for SCA LTV phase computation.
 *
 * Called after each received or missed TD frame to advance the window to the
 * next expected arrival time.  @p aSlwSampleTime is in microseconds on the
 * local radio clock (see `otPlatRadioGetNow()`).
 *
 * @param[in] aInstance      The OpenThread instance.
 * @param[in] aSlwSampleTime Next expected frame arrival time in us.
 */
void otPlatRadioUpdateThreadDirectSlwSampleTime(otInstance *aInstance, uint32_t aSlwSampleTime);

/**
 * Returns the worst-case clock accuracy of the local radio in PPM for Thread
 * Direct SLW transmit scheduling.
 *
 * Used by the WI to calculate the guard time when scheduling a unicast TX to
 * arrive within the WL's SLW window.
 *
 * @param[in] aInstance  The OpenThread instance.
 *
 * @returns Worst-case clock accuracy in PPM.
 */
uint8_t otPlatRadioGetThreadDirectSlwAccuracy(otInstance *aInstance);

/**
 * Returns the fixed arrival-time uncertainty for Thread Direct SLW frames in
 * units of 10 us.
 *
 * Used by the WI alongside `otPlatRadioGetThreadDirectSlwAccuracy()` to compute
 * the total guard window.
 *
 * @param[in] aInstance  The OpenThread instance.
 *
 * @returns Fixed SLW arrival-time uncertainty in units of 10 us.
 */
uint8_t otPlatRadioGetThreadDirectSlwUncertainty(otInstance *aInstance);

/**
 * Retrieves the current Radio Availability Mask (RAM) parameters from the
 * platform radio driver.
 *
 * Called when `OPENTHREAD_CONFIG_THREAD_DIRECT_COEX_ENABLE` is set.  The
 * platform fills @p aParams with the current RAM availability state and, when
 * available, the current CoEx constraints (bitmap of unavailable slots within
 * the SLW period, signed offset, and RAM Duration code). Platforms without
 * CoEx support may return `OT_ERROR_NOT_IMPLEMENTED`.
 *
 * @param[in]  aInstance  The OpenThread instance.
 * @param[out] aParams    Filled with current CoEx constraints.
 *
 * @retval OT_ERROR_NONE             @p aParams populated.
 * @retval OT_ERROR_NOT_IMPLEMENTED  Feature is not implemented.
 */
otError otPlatRadioGetThreadDirectRamParams(otInstance *aInstance, otThreadDirectRamParams *aParams);

/**
 * @}
 *
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENTHREAD_PLATFORM_THREAD_DIRECT_H_
