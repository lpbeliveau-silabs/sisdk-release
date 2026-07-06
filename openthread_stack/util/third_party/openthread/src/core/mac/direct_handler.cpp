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
 *   This file implements the Thread Direct handler: WI and WL link handshake
 *   state machines and local SCA state management.
 */

#include "direct_handler.hpp"

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

#include <string.h>

#include "common/code_utils.hpp"
#include "common/frame_builder.hpp"
#include "common/log.hpp"
#include "common/random.hpp"
#include "common/time.hpp"
#include "config/thread_direct.h"
#include "instance/instance.hpp"
#include "mac/mac.hpp"
#include "mac/mac_links.hpp"
#include "mac/sub_mac.hpp"
#include "radio/radio.hpp"
#include "thread/direct_peer.hpp"
#include "thread/direct_peer_table.hpp"
#include "thread/key_manager.hpp"

namespace ot {

RegisterLogModule("DirectHandler");

namespace {

uint32_t ScaSlotDurationToUs(Mac::ScaSlotDuration aSlotDuration)
{
    switch (aSlotDuration)
    {
    case Mac::ScaSlotDuration::k625Usec:
        return 625;
    case Mac::ScaSlotDuration::k1250Usec:
        return 1250;
    case Mac::ScaSlotDuration::k625Msec:
        return 625000;
    case Mac::ScaSlotDuration::k1250Msec:
        return 1250000;
    }

    OT_UNREACHABLE_CODE(return 625;);
}

} // namespace

DirectHandler::DirectHandler(Instance &aInstance)
    : InstanceLocator(aInstance)
#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
    , mWlState(kWlIdle)
    , mWlAttachDelayTimer(aInstance)
#endif
    , mSlwSlotDurationUs(0)
    , mSlwPeriodUs(0)
    , mSlwTimeout(kDefaultSlwTimeout)
#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
    , mTeardownKeyIndex(Mac::Frame::kWakeKeyIndex)
    , mTeardownPending(false)
#endif
{
    memset(&mLocalSca, 0, sizeof(mLocalSca));
    mLocalSca.mSlotDuration = Mac::ScaSlotDuration::k625Usec;
    mLocalSca.mRamAvailable = false;
    UpdateDerivedSlwTiming();
}

Error DirectHandler::SetSlwSchedule(uint16_t aPeriodSlots)
{
    Error error = kErrorNone;

    if (aPeriodSlots != 0)
    {
        VerifyOrExit(aPeriodSlots >= OPENTHREAD_CONFIG_THREAD_DIRECT_SLW_MIN_DURATION_SLOTS, error = kErrorInvalidArgs);
    }

    mLocalSca.mSlwPeriodSlots = aPeriodSlots;
    mLocalSca.mSlwPhaseSlots  = 0; // Phase is stack-computed at frame-build time.
    mLocalSca.mHasSlw         = (aPeriodSlots != 0);
    UpdateDerivedSlwTiming();

exit:
    return error;
}

void DirectHandler::GetSlwSchedule(uint16_t &aPeriodSlots) const { aPeriodSlots = mLocalSca.mSlwPeriodSlots; }

Error DirectHandler::SetRamMask(const Mac::ScaParams &aParams)
{
    Error error = kErrorNone;

    VerifyOrExit(aParams.mRamOffsetUs >= Mac::ScaParams::kRamOffsetUsMin &&
                     aParams.mRamOffsetUs <= Mac::ScaParams::kRamOffsetUsMax,
                 error = kErrorInvalidArgs);

    mLocalSca.mRamAvailable = aParams.mRamAvailable;
    mLocalSca.mRamDuration  = aParams.mRamDuration;
    mLocalSca.mRamOffsetUs  = aParams.mRamOffsetUs;
    mLocalSca.mSlotDuration = aParams.mSlotDuration;
    memcpy(mLocalSca.mRamBits, aParams.mRamBits, sizeof(mLocalSca.mRamBits));
    UpdateDerivedSlwTiming();

exit:
    return error;
}

void DirectHandler::GetRamMask(Mac::ScaParams &aParams) const
{
    aParams.mRamAvailable = mLocalSca.mRamAvailable;
    aParams.mRamDuration  = mLocalSca.mRamDuration;
    aParams.mRamOffsetUs  = mLocalSca.mRamOffsetUs;
    aParams.mSlotDuration = mLocalSca.mSlotDuration;
    memcpy(aParams.mRamBits, mLocalSca.mRamBits, sizeof(aParams.mRamBits));
}

Error DirectHandler::SetSlwTimeout(uint32_t aTimeout)
{
    Error error = kErrorNone;

    VerifyOrExit(aTimeout <= kMaxSlwTimeout, error = kErrorInvalidArgs);
    mSlwTimeout = (aTimeout == 0) ? kDefaultSlwTimeout : aTimeout;

exit:
    return error;
}

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

Error DirectHandler::Unlink(const Mac::ExtAddress &aExtAddress)
{
    Error       error = kErrorNone;
    DirectPeer *peer  = Get<DirectPeerTable>().FindPeer(aExtAddress, Neighbor::kInStateValid);

    VerifyOrExit(peer != nullptr, error = kErrorNotFound);

    mTeardownKeyIndex = peer->GetWakeKeyIndex();
    peer->Clear();
    Get<Mac::Mac>().RefreshThreadDirectSlwScheduling();
    LogInfo("TD: unlinked from %s, sending teardown", aExtAddress.ToString().AsCString());

    Get<Mac::Mac>().InvokeDirectEvent(OT_THREAD_DIRECT_EVENT_UNLINKED, nullptr);

    mTeardownAddr    = aExtAddress;
    mTeardownPending = true;
    Get<Mac::Mac>().RequestTeardownTransmission();

exit:
    return error;
}

Mac::TxFrame *DirectHandler::PrepareTeardownFrame(Mac::TxFrames &aTxFrames)
{
    Mac::TxFrame *frame = nullptr;

    VerifyOrExit(mTeardownPending);
    mTeardownPending = false;

    Get<Mac::SubMac>().SetActiveBurstWakeKeyIndex(mTeardownKeyIndex);

#if OPENTHREAD_CONFIG_MULTI_RADIO
    frame = &aTxFrames.GetTxFrame(Mac::kRadioTypeIeee802154);
#else
    frame = &aTxFrames.GetTxFrame();
#endif

    if (frame->GenerateThreadDirectTeardown(Get<Mac::Mac>().GetPanId(), mTeardownAddr,
                                            Get<Mac::Mac>().GetExtAddress()) != kErrorNone)
    {
        LogWarn("TD: teardown frame build failed");
        frame = nullptr;
    }
    else
    {
        frame->SetCsmaCaEnabled(true);
        frame->SetMaxCsmaBackoffs(Mac::kMaxCsmaBackoffsDirect);
        frame->SetMaxFrameRetries(0);
    }

exit:
    return frame;
}

void DirectHandler::HandleTeardownRxd(const Mac::ExtAddress &aPeerAddr)
{
    DirectPeer *peer = Get<DirectPeerTable>().FindPeer(aPeerAddr, Neighbor::kInStateValid);

    if (peer != nullptr)
    {
        peer->Clear();
        Get<Mac::Mac>().RefreshThreadDirectSlwScheduling();
        LogInfo("TD: received teardown from %s", aPeerAddr.ToString().AsCString());
        Get<Mac::Mac>().InvokeDirectEvent(OT_THREAD_DIRECT_EVENT_UNLINKED, nullptr);
    }
    else
    {
        LogWarn("TD: teardown from unknown peer %s", aPeerAddr.ToString().AsCString());
    }
}

#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

void DirectHandler::HandleWakeReceived(const Mac::WakeupInfo &aWakeupInfo)
{
    // Drop if already handling a handshake (a second WI won the race).
    VerifyOrExit(mWlState == kWlIdle);

    mWakeupInfo = aWakeupInfo;
    mWlState    = kWlAttachDelay;

    {
        uint32_t delayMs = (aWakeupInfo.mAttachDelayUs + Time::kOneMsecInUsec - 1) / Time::kOneMsecInUsec;

        LogInfo("TD WL: wake received from %s, attach delay %lums", aWakeupInfo.mExtAddress.ToString().AsCString(),
                ToUlong(delayMs));

        if (delayMs == 0)
        {
            mWlAttachDelayTimer.FireAt(TimerMilli::GetNow());
        }
        else
        {
            mWlAttachDelayTimer.FireAt(TimerMilli::GetNow() + delayMs);
        }
    }

exit:
    return;
}

void DirectHandler::HandleWlAttachDelayTimer(void)
{
    VerifyOrExit(mWlState == kWlAttachDelay);

    mWlState = kWlWaitingEnhAck;
    Get<Mac::SubMac>().SetActiveBurstWakeKeyIndex(mWakeupInfo.mWakeKeyIndex);

    Get<Mac::Mac>().RequestTdLinkCmdTransmission();

exit:
    return;
}

Mac::TxFrame *DirectHandler::PrepareTdLinkCmdFrame(Mac::TxFrames &aTxFrames)
{
    Mac::TxFrame *frame = nullptr;
    Error         error = kErrorNone;

    VerifyOrExit(mWlState == kWlWaitingEnhAck);

    // Compute the challenge now, using the wake frame counter that
    // ProcessTransmitSecurity will stamp in the TD Link Command.
    SuccessOrExit(error = Get<KeyManager>().ComputeChallenge(mWakeupInfo.mWakeKeyIndex,
                                                             Get<Mac::SubMac>().GetWakeFrameCounter(),
                                                             mWakeupInfo.mWakeFrameCounter, nullptr, 0, mWlChallenge));

#if OPENTHREAD_CONFIG_MULTI_RADIO
    frame = &aTxFrames.GetTxFrame(Mac::kRadioTypeIeee802154);
#else
    frame = &aTxFrames.GetTxFrame();
#endif

    SuccessOrExit(error = frame->GenerateThreadDirectLinkCommand(Get<Mac::Mac>().GetPanId(), mWakeupInfo.mExtAddress,
                                                                 Get<Mac::Mac>().GetExtAddress(), GetLocalSca(),
                                                                 mWlChallenge, nullptr, nullptr));

    frame->SetCsmaCaEnabled(true);
    frame->SetMaxCsmaBackoffs(Mac::kMaxCsmaBackoffsDirect);
    frame->SetMaxFrameRetries(0);

exit:
    if (error != kErrorNone)
    {
        LogWarn("TD WL: link command preparation failed: %s", ErrorToString(error));
        mWlState = kWlIdle;
        Get<Mac::SubMac>().SetActiveBurstWakeKeyIndex(Mac::Frame::kWakeKeyIndex);
        ResumeWakeListening();
        frame = nullptr;
    }

    return frame;
}

void DirectHandler::HandleTdLinkCmdTxDone(Mac::TxFrame &aFrame, Mac::RxFrame *aAckFrame, Error aError)
{
    OT_UNUSED_VARIABLE(aFrame);

    Error error = aError;

    VerifyOrExit(mWlState == kWlWaitingEnhAck);
    Get<Mac::SubMac>().SetActiveBurstWakeKeyIndex(Mac::Frame::kWakeKeyIndex);

    VerifyOrExit(error == kErrorNone, LogWarn("TD WL: TD Link Command TX failed: %s", ErrorToString(error)));
    VerifyOrExit(aAckFrame != nullptr, error = kErrorNoAck);

    {
        const uint8_t    *ieData;
        uint8_t           ieLen;
        Mac::ChallengeLtv echoed;
        Mac::ScaParams    wiSca;

        memset(&echoed, 0, sizeof(echoed));
        memset(&wiSca, 0, sizeof(wiSca));

        ieData = aAckFrame->GetHeaderIe(Mac::ThreadHeaderIe::kElementId);
        VerifyOrExit(ieData != nullptr, error = kErrorNotFound);

        ieLen = reinterpret_cast<const Mac::HeaderIe *>(ieData)->GetLength();

        VerifyOrExit(Mac::ParseThreadHeaderIe(ieData + sizeof(Mac::HeaderIe), ieLen, &wiSca, &echoed) == kErrorNone,
                     error = kErrorParse);

        VerifyOrExit(memcmp(echoed.mChallenge, mWlChallenge.mChallenge, Mac::ChallengeLtv::kLength) == 0,
                     error = kErrorSecurity);

        {
            DirectPeer *peer = Get<DirectPeerTable>().FindPeer(mWakeupInfo.mExtAddress, DirectPeer::kInStateAny);

            if (peer == nullptr)
            {
                peer = Get<DirectPeerTable>().GetNewPeer();
                VerifyOrExit(peer != nullptr, error = kErrorNoBufs);
            }

            peer->SetExtAddress(mWakeupInfo.mExtAddress);
            peer->SetState(Neighbor::kStateValid);
            peer->SetWakeKeyIndex(mWakeupInfo.mWakeKeyIndex);
            peer->SetWakeKeyUsed(true);
            peer->SetLastWakeFrameCounter(mWakeupInfo.mWakeFrameCounter);

            if (wiSca.mHasSlw)
            {
                peer->SetSlwPeriodSlots(wiSca.mSlwPeriodSlots);
                peer->SetSlwPhaseSlots(wiSca.mSlwPhaseSlots);
                peer->SetHasScaSchedule(true);
                peer->SetCoexEnabled(wiSca.mRamAvailable);
            }

            Get<Mac::Mac>().RefreshThreadDirectSlwScheduling();
            LogInfo("TD WL: linked with %s", mWakeupInfo.mExtAddress.ToString().AsCString());
        }

        mWlState = kWlIdle;
        {
            otThreadDirectPeerInfo peerInfo;

            ClearAllBytes(peerInfo);
            static_cast<Mac::ExtAddress &>(peerInfo.mExtAddress) = mWakeupInfo.mExtAddress;
            Get<Mac::Mac>().InvokeDirectEvent(OT_THREAD_DIRECT_EVENT_LINKED, &peerInfo);
        }
        ExitNow();
    }

exit:
    if (mWlState == kWlWaitingEnhAck)
    {
        mWlState = kWlIdle;

        if (error != kErrorNone)
        {
            LogWarn("TD WL: handshake failed (%s), resuming wake listen", ErrorToString(error));
            ResumeWakeListening();
        }
    }
}

void DirectHandler::ResumeWakeListening(void)
{
    uint32_t jitterMs = Random::NonCrypto::GetUint32InRange(
        0, OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INTERVAL_US / Time::kOneMsecInUsec + 1);

    OT_UNUSED_VARIABLE(jitterMs);

    IgnoreError(Get<Mac::Mac>().SetWakeupListenEnabled(true));
}

#if !OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE
void DirectHandler::HandleTdDirectFrame(const Mac::RxFrame &aFrame)
{
    Mac::Address   srcAddress;
    const uint8_t *ieData;
    uint8_t        ieLen;
    Mac::ScaParams sca;

    SuccessOrExit(aFrame.GetSrcAddr(srcAddress));
    VerifyOrExit(srcAddress.IsExtended());

    ieData = aFrame.GetHeaderIe(Mac::ThreadHeaderIe::kElementId);
    VerifyOrExit(ieData != nullptr);

    ieLen = reinterpret_cast<const Mac::HeaderIe *>(ieData)->GetLength();

    {
        bool teardown = false;

        memset(&sca, 0, sizeof(sca));
        IgnoreError(Mac::ParseThreadHeaderIe(ieData + sizeof(Mac::HeaderIe), ieLen, &sca, nullptr, &teardown));

        if (teardown)
        {
            HandleTeardownRxd(srcAddress.GetExtended());
        }
    }

exit:
    return;
}
#endif // !OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE

#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE

void DirectHandler::HandleTdLinkCommand(const Mac::RxFrame &aFrame)
{
    Error             error = kErrorNone;
    Mac::Address      srcAddress;
    const uint8_t    *ieData;
    uint8_t           ieLen;
    Mac::ScaParams    wlSca;
    Mac::ChallengeLtv receivedChallenge;
    uint8_t           wakeKeyIndex;
    uint32_t          linkFrameCounter;

    Get<WakeupTxScheduler>().NotifyTdLinkCommandReceived();

    SuccessOrExit(error = aFrame.GetSrcAddr(srcAddress));
    VerifyOrExit(srcAddress.IsExtended(), error = kErrorDrop);

    SuccessOrExit(error = aFrame.GetKeyId(wakeKeyIndex));
    SuccessOrExit(error = aFrame.GetFrameCounter(linkFrameCounter));

    ieData = aFrame.GetHeaderIe(Mac::ThreadHeaderIe::kElementId);
    VerifyOrExit(ieData != nullptr, error = kErrorNotFound);

    ieLen = reinterpret_cast<const Mac::HeaderIe *>(ieData)->GetLength();

    {
        bool teardown         = false;
        bool challengePresent = false;

        memset(&wlSca, 0, sizeof(wlSca));
        memset(&receivedChallenge, 0, sizeof(receivedChallenge));
        VerifyOrExit(Mac::ParseThreadHeaderIe(ieData + sizeof(Mac::HeaderIe), ieLen, &wlSca, &receivedChallenge,
                                              &teardown, &challengePresent) == kErrorNone,
                     error = kErrorParse);

        if (teardown)
        {
            HandleTeardownRxd(srcAddress.GetExtended());
            ExitNow();
        }

        VerifyOrExit(challengePresent, error = kErrorNotFound);
    }

    {
        DirectPeer *peer = Get<DirectPeerTable>().FindPeer(srcAddress.GetExtended(), DirectPeer::kInStateAny);

        if (peer == nullptr)
        {
            peer = Get<DirectPeerTable>().GetNewPeer();
            VerifyOrExit(peer != nullptr, error = kErrorNoBufs);
        }
        else if (peer->GetState() == Neighbor::kStateValid)
        {
            // Already linked with this peer; re-echo the challenge so
            // the WL can complete its own handshake on retry.
            LogInfo("TD WI: re-echoing challenge for already-linked peer %s",
                    srcAddress.GetExtended().ToString().AsCString());
            ExitNow();
        }
        peer->SetExtAddress(srcAddress.GetExtended());
        peer->SetState(Neighbor::kStateValid);
        peer->SetWakeKeyIndex(wakeKeyIndex);
        peer->SetWakeKeyUsed(true);
        peer->SetLastWakeFrameCounter(linkFrameCounter);

        if (wlSca.mHasSlw)
        {
            peer->SetSlwPeriodSlots(wlSca.mSlwPeriodSlots);
            peer->SetSlwPhaseSlots(wlSca.mSlwPhaseSlots);
            peer->SetHasScaSchedule(true);
            peer->SetCoexEnabled(wlSca.mRamAvailable);
        }

        Get<Mac::Mac>().RefreshThreadDirectSlwScheduling();
        LogInfo("TD WI: linked with %s", srcAddress.GetExtended().ToString().AsCString());
    }

    Get<WakeupTxScheduler>().Stop();

    {
        otThreadDirectPeerInfo peerInfo;

        ClearAllBytes(peerInfo);
        static_cast<Mac::ExtAddress &>(peerInfo.mExtAddress) = srcAddress.GetExtended();
        Get<Mac::Mac>().InvokeDirectEvent(OT_THREAD_DIRECT_EVENT_LINKED, &peerInfo);
    }

exit:
    if (error != kErrorNone)
    {
        LogWarn("TD WI: link handshake failed (%s)", ErrorToString(error));
        Get<Mac::Mac>().InvokeDirectEvent(OT_THREAD_DIRECT_EVENT_LINK_FAILED, nullptr);
    }
}

#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE

void DirectHandler::UpdateDerivedSlwTiming(void)
{
    mSlwSlotDurationUs = ScaSlotDurationToUs(mLocalSca.mSlotDuration);
    mSlwPeriodUs       = static_cast<uint64_t>(mSlwSlotDurationUs) * mLocalSca.mSlwPeriodSlots;
}

} // namespace ot

#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
