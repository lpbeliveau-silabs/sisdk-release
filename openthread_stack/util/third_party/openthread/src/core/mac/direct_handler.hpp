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
 *   This file includes definitions for the Thread Direct handler.
 *
 *   Implements the Wake Initiator (WI) and Wake Listener (WL) link handshake
 *   state machines.  The WL state machine drives the TD Link Command TX path;
 *   the WI state machine processes incoming TD Link Commands and arms the
 *   Enh-ACK Thread Header IE.
 */

#ifndef OT_CORE_MAC_DIRECT_HANDLER_HPP_
#define OT_CORE_MAC_DIRECT_HANDLER_HPP_

#include "openthread-core-config.h"

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

#include "common/error.hpp"
#include "common/locator.hpp"
#include "common/non_copyable.hpp"
#include "common/timer.hpp"
#include "mac/mac_frame.hpp"
#include "mac/mac_header_ie.hpp"
#include "mac/mac_links.hpp"
#include "mac/mac_types.hpp"

namespace ot {

/**
 * Manages the Thread Direct link handshake for both the Wake Initiator and
 * Wake Listener roles, and the local SCA state (SLW schedule and RAM
 * parameters) advertised in outgoing SCA LTVs.
 */
class DirectHandler : public InstanceLocator, private NonCopyable
{
public:
    explicit DirectHandler(Instance &aInstance);

    /**
     * Sets the SLW (Scheduled Listen Window) period advertised by this device.
     *
     * Phase is stack-computed at frame-build time and is not configurable here.
     *
     * @param[in] aPeriodSlots  SLW period in units of the configured Slot Duration.
     *                          Pass 0 to clear the schedule / advertise rx-on-when-idle.
     *
     * @retval kErrorNone  Schedule stored.
     */
    Error SetSlwSchedule(uint16_t aPeriodSlots);

    /**
     * Gets the SLW (Scheduled Listen Window) period configured on this device.
     *
     * @param[out] aPeriodSlots  Set to the SLW period in units of the configured
     *                           Slot Duration (0 if not configured).
     */
    void GetSlwSchedule(uint16_t &aPeriodSlots) const;

    /**
     * Sets the RAM (Radio Availability Mask) parameters advertised by this device.
     *
     * @param[in] aParams  RAM parameters to store.
     *
     * @retval kErrorNone         Parameters stored.
     * @retval kErrorInvalidArgs  @p aParams.mRamOffsetUs is outside [-1024, 1023].
     */
    Error SetRamMask(const Mac::ScaParams &aParams);

    /**
     * Gets the RAM (Radio Availability Mask) parameters configured on this device.
     *
     * @param[out] aParams  Set to the RAM parameters.
     */
    void GetRamMask(Mac::ScaParams &aParams) const;

    /**
     * Gets the local SCA state (SLW schedule and RAM parameters).
     *
     * @returns A const reference to the local SCA parameters.
     */
    const Mac::ScaParams &GetLocalSca(void) const { return mLocalSca; }

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
    /**
     * Tears down the TD link with @p aExtAddress.
     *
     * Removes the `DirectPeer` entry, fires `OT_THREAD_DIRECT_EVENT_UNLINKED`, and schedules
     * a best-effort teardown frame (empty SCA LTV).
     *
     * @param[in] aExtAddress  Extended address of the peer to unlink.
     *
     * @retval kErrorNone      Teardown initiated.
     * @retval kErrorNotFound  No active link to @p aExtAddress.
     */
    Error Unlink(const Mac::ExtAddress &aExtAddress);

    /**
     * Builds the Thread Direct teardown frame in @p aTxFrames.
     *
     * Called by `Mac::BeginTransmit` for `kOperationTransmitTdTeardown`.
     *
     * @param[in,out] aTxFrames  MAC TX frame set.
     *
     * @returns Pointer to the prepared `TxFrame`, or `nullptr` on error.
     */
    Mac::TxFrame *PrepareTeardownFrame(Mac::TxFrames &aTxFrames);
#endif

    /**
     * Indicates whether a local SLW schedule is configured.
     *
     * @retval TRUE   Local SLW scheduling is configured.
     * @retval FALSE  Local SLW scheduling is disabled.
     */
    bool HasSlwSchedule(void) const { return mSlwPeriodUs != 0; }

    /**
     * Gets the configured local SLW Slot Duration in microseconds.
     *
     * @returns The configured Slot Duration in microseconds.
     */
    uint32_t GetSlwSlotDurationUs(void) const { return mSlwSlotDurationUs; }

    /**
     * Gets the configured local SLW period in microseconds.
     *
     * @returns The configured SLW period in microseconds.
     */
    uint64_t GetSlwPeriodUs(void) const { return mSlwPeriodUs; }

    /**
     * Returns the SLW link inactivity timeout in seconds.
     *
     * @returns Current SLW timeout (seconds).
     */
    uint32_t GetSlwTimeout(void) const { return mSlwTimeout; }

    /**
     * Sets the SLW link inactivity timeout in seconds.
     *
     * @param[in] aTimeout  Timeout in seconds; 0 restores the compile-time default.
     *
     * @retval kErrorNone         Timeout stored.
     * @retval kErrorInvalidArgs  @p aTimeout exceeds kMaxSlwTimeout.
     */
    Error SetSlwTimeout(uint32_t aTimeout);

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
    /**
     * Called by `Mac::HandleWakeupFrame` after a TD Wake Command is received.
     *
     * @param[in] aWakeupInfo  Decoded fields from the received Wake Command.
     */
    void HandleWakeReceived(const Mac::WakeupInfo &aWakeupInfo);

    /**
     * Called by `Mac::BeginTransmit` for `kOperationTransmitTdLinkCmd`.
     *
     * @param[in,out] aTxFrames  The MAC TX frame set.
     *
     * @returns A pointer to the prepared `TxFrame`, or `nullptr` on error.
     */
    Mac::TxFrame *PrepareTdLinkCmdFrame(Mac::TxFrames &aTxFrames);

    /**
     * Called by `Mac::HandleTransmitDone` after the TD Link Command TX completes.
     *
     * @param[in] aFrame     The transmitted TD Link Command frame.
     * @param[in] aAckFrame  The received Enh-ACK frame, or `nullptr` if none.
     * @param[in] aError     TX result (`kErrorNone` on success).
     */
    void HandleTdLinkCmdTxDone(Mac::TxFrame &aFrame, Mac::RxFrame *aAckFrame, Error aError);
#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE
    /**
     * Called by `Mac::HandleMacCommand` when a TD Link Command is received.
     *
     * Arms the Enh-ACK IE immediately (platform has ~192 us to inject it into
     * the hardware ACK).  An empty SCA LTV in the payload is treated as teardown.
     *
     * @param[in] aFrame  The received TD Link Command frame (already decrypted).
     */
    void HandleTdLinkCommand(const Mac::RxFrame &aFrame);
#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE && !OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE
    /**
     * Called by `Mac::HandleMacCommand` for TD Direct frames (MAC Cmd 0x54 /
     * Thread Cmd 0x02) in WL-only builds.
     *
     * Handles teardown frames from the WI (empty SCA LTV, no Challenge LTV).
     *
     * @param[in] aFrame  The received frame (already decrypted).
     */
    void HandleTdDirectFrame(const Mac::RxFrame &aFrame);
#endif

private:
    static constexpr uint32_t kDefaultSlwTimeout = OPENTHREAD_CONFIG_THREAD_DIRECT_SLW_TIMEOUT;
    static constexpr uint32_t kMaxSlwTimeout     = OPENTHREAD_CONFIG_THREAD_DIRECT_SLW_MAX_TIMEOUT;

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
    void HandleTeardownRxd(const Mac::ExtAddress &aPeerAddr);
#endif

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
    enum WlState : uint8_t
    {
        kWlIdle,          ///< No handshake in progress.
        kWlAttachDelay,   ///< Waiting for rendezvous time before sending TD Link Command.
        kWlWaitingEnhAck, ///< TD Link Command TX pending or sent; waiting for Enh-ACK.
    };

    void HandleWlAttachDelayTimer(void);
    void ResumeWakeListening(void);

    using AttachDelayTimer = TimerMilliIn<DirectHandler, &DirectHandler::HandleWlAttachDelayTimer>;

    WlState           mWlState;
    Mac::WakeupInfo   mWakeupInfo;
    Mac::ChallengeLtv mWlChallenge;
    AttachDelayTimer  mWlAttachDelayTimer;
#endif
    void UpdateDerivedSlwTiming(void);

    Mac::ScaParams mLocalSca;
    uint32_t       mSlwSlotDurationUs;
    uint64_t       mSlwPeriodUs;
    uint32_t       mSlwTimeout;
#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
    Mac::ExtAddress mTeardownAddr;
    uint8_t         mTeardownKeyIndex;
    bool            mTeardownPending;
#endif
};

} // namespace ot

#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

#endif // OT_CORE_MAC_DIRECT_HANDLER_HPP_
