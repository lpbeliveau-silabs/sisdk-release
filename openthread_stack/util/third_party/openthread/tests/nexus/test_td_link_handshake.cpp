/*
 *  Copyright (c) 2026, The OpenThread Authors.
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

#include <stdio.h>
#include <string.h>

#include <openthread/thread_direct.h>

#include "platform/nexus_core.hpp"
#include "platform/nexus_node.hpp"

namespace ot {
namespace Nexus {

static constexpr uint32_t kHandshakeTimeMs = 5 * 1000;
static constexpr uint32_t kTeardownTimeMs  = 2 * 1000;

static constexpr uint8_t  kNetworkKey[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                                              0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
static constexpr uint8_t  kGuestKey[OT_NETWORK_KEY_SIZE]   = {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18,
                                                              0x29, 0x3a, 0x4b, 0x5c, 0x6d, 0x7e, 0x8f, 0x90};
static constexpr uint8_t  kGuestKeyIndex                   = 130;
static constexpr uint16_t kPanId                           = 0xD001;

struct TdEventInfo
{
    uint32_t            mLinkedCount;
    uint32_t            mUnlinkedCount;
    uint32_t            mWakeReceivedCount;
    otThreadDirectEvent mLastEvent;
    otExtAddress        mLastPeerAddr;
};

static void HandleTdEvent(otThreadDirectEvent aEvent, const otThreadDirectPeerInfo *aPeerInfo, void *aContext)
{
    TdEventInfo *info = static_cast<TdEventInfo *>(aContext);

    info->mLastEvent = aEvent;

    if (aPeerInfo != nullptr)
    {
        info->mLastPeerAddr = aPeerInfo->mExtAddress;
    }

    switch (aEvent)
    {
    case OT_THREAD_DIRECT_EVENT_LINKED:
        info->mLinkedCount++;
        break;
    case OT_THREAD_DIRECT_EVENT_UNLINKED:
        info->mUnlinkedCount++;
        break;
    case OT_THREAD_DIRECT_EVENT_WAKE_RECEIVED:
        info->mWakeReceivedCount++;
        break;
    default:
        break;
    }
}

void TestTdLinkHandshake(void)
{
    Core nexus;

    Node &wi = nexus.CreateNode();
    Node &wl = nexus.CreateNode();

    TdEventInfo wiEvents;
    TdEventInfo wlEvents;

    memset(&wiEvents, 0, sizeof(wiEvents));
    memset(&wlEvents, 0, sizeof(wlEvents));

    wi.SetName("WI");
    wl.SetName("WL");

    AllowLinkBetween(wi, wl);

    nexus.AdvanceTime(0);

    SuccessOrQuit(Instance::SetGlobalLogLevel(kLogLevelDebg));

    Log("---------------------------------------------------------------------------------------");
    Log("Step 1: Configure both nodes with a shared network key (detached, no Thread network)");

    otNetworkKey networkKey;
    memcpy(networkKey.m8, kNetworkKey, sizeof(kNetworkKey));

    SuccessOrQuit(otThreadSetNetworkKey(&wi.GetInstance(), &networkKey));
    SuccessOrQuit(otThreadSetNetworkKey(&wl.GetInstance(), &networkKey));

    SuccessOrQuit(otLinkSetPanId(&wi.GetInstance(), kPanId));
    SuccessOrQuit(otLinkSetPanId(&wl.GetInstance(), kPanId));

    SuccessOrQuit(otIp6SetEnabled(&wi.GetInstance(), true));
    SuccessOrQuit(otIp6SetEnabled(&wl.GetInstance(), true));

    nexus.AdvanceTime(100);

    Log("---------------------------------------------------------------------------------------");
    Log("Step 2: Register TD event callbacks and enable WL wake listen");

    otThreadDirectSetEventCallback(&wi.GetInstance(), HandleTdEvent, &wiEvents);
    otThreadDirectSetEventCallback(&wl.GetInstance(), HandleTdEvent, &wlEvents);

    SuccessOrQuit(otThreadDirectWakeListenerEnable(&wl.GetInstance(), true));
    VerifyOrQuit(otThreadDirectIsWakeListenerEnabled(&wl.GetInstance()));

    nexus.AdvanceTime(100);

    Log("---------------------------------------------------------------------------------------");
    Log("Step 3: WI starts wake burst targeting WL");

    {
        const otExtAddress &wlAddr = *reinterpret_cast<const otExtAddress *>(&wl.mRadio.mExtAddress);

        SuccessOrQuit(otThreadDirectWakeup(&wi.GetInstance(), &wlAddr, OT_THREAD_DIRECT_WAKE_TYPE_LINK, 0, 0, 0));
        VerifyOrQuit(otThreadDirectIsWakeBurstActive(&wi.GetInstance()));
    }

    Log("---------------------------------------------------------------------------------------");
    Log("Step 4: Advance time for handshake to complete");

    nexus.AdvanceTime(kHandshakeTimeMs);

    Log("---------------------------------------------------------------------------------------");
    Log("Step 5: Verify both sides received LINKED event");

    VerifyOrQuit(wiEvents.mLinkedCount == 1);
    VerifyOrQuit(wlEvents.mLinkedCount == 1);
    VerifyOrQuit(wlEvents.mWakeReceivedCount >= 1);

    {
        const otExtAddress &wlAddr = *reinterpret_cast<const otExtAddress *>(&wl.mRadio.mExtAddress);
        const otExtAddress &wiAddr = *reinterpret_cast<const otExtAddress *>(&wi.mRadio.mExtAddress);

        VerifyOrQuit(memcmp(wiEvents.mLastPeerAddr.m8, wlAddr.m8, OT_EXT_ADDRESS_SIZE) == 0);
        VerifyOrQuit(memcmp(wlEvents.mLastPeerAddr.m8, wiAddr.m8, OT_EXT_ADDRESS_SIZE) == 0);
    }

    Log("---------------------------------------------------------------------------------------");
    Log("Step 6: WI initiates teardown");

    {
        const otExtAddress &wlAddr = *reinterpret_cast<const otExtAddress *>(&wl.mRadio.mExtAddress);

        SuccessOrQuit(otThreadDirectUnlink(&wi.GetInstance(), &wlAddr));
    }

    nexus.AdvanceTime(kTeardownTimeMs);

    Log("---------------------------------------------------------------------------------------");
    Log("Step 7: Verify both sides received UNLINKED event");

    VerifyOrQuit(wiEvents.mUnlinkedCount == 1);
    VerifyOrQuit(wlEvents.mUnlinkedCount == 1);
}

void TestTdLinkHandshakeGuestKey(void)
{
    Core nexus;

    Node &wi = nexus.CreateNode();
    Node &wl = nexus.CreateNode();

    TdEventInfo wiEvents;
    TdEventInfo wlEvents;

    memset(&wiEvents, 0, sizeof(wiEvents));
    memset(&wlEvents, 0, sizeof(wlEvents));

    wi.SetName("WI");
    wl.SetName("WL");

    AllowLinkBetween(wi, wl);

    nexus.AdvanceTime(0);

    SuccessOrQuit(Instance::SetGlobalLogLevel(kLogLevelDebg));

    Log("---------------------------------------------------------------------------------------");
    Log("Step 1: Configure both nodes with a shared network key and guest wake key at index 130");

    otNetworkKey networkKey;
    memcpy(networkKey.m8, kNetworkKey, sizeof(kNetworkKey));

    SuccessOrQuit(otThreadSetNetworkKey(&wi.GetInstance(), &networkKey));
    SuccessOrQuit(otThreadSetNetworkKey(&wl.GetInstance(), &networkKey));

    SuccessOrQuit(otLinkSetPanId(&wi.GetInstance(), kPanId));
    SuccessOrQuit(otLinkSetPanId(&wl.GetInstance(), kPanId));

    {
        otThreadDirectWakeKey guestKey;
        memcpy(guestKey.m8, kGuestKey, sizeof(guestKey.m8));

        SuccessOrQuit(otThreadDirectSetGuestWakeKey(&wi.GetInstance(), kGuestKeyIndex, &guestKey));
        SuccessOrQuit(otThreadDirectSetGuestWakeKey(&wl.GetInstance(), kGuestKeyIndex, &guestKey));
    }

    SuccessOrQuit(otIp6SetEnabled(&wi.GetInstance(), true));
    SuccessOrQuit(otIp6SetEnabled(&wl.GetInstance(), true));

    nexus.AdvanceTime(100);

    Log("---------------------------------------------------------------------------------------");
    Log("Step 2: Register TD event callbacks and enable WL wake listen");

    otThreadDirectSetEventCallback(&wi.GetInstance(), HandleTdEvent, &wiEvents);
    otThreadDirectSetEventCallback(&wl.GetInstance(), HandleTdEvent, &wlEvents);

    SuccessOrQuit(otThreadDirectWakeListenerEnable(&wl.GetInstance(), true));
    VerifyOrQuit(otThreadDirectIsWakeListenerEnabled(&wl.GetInstance()));

    nexus.AdvanceTime(100);

    Log("---------------------------------------------------------------------------------------");
    Log("Step 3: WI starts wake burst using guest key index 130");

    {
        const otExtAddress &wlAddr = *reinterpret_cast<const otExtAddress *>(&wl.mRadio.mExtAddress);

        SuccessOrQuit(
            otThreadDirectWakeup(&wi.GetInstance(), &wlAddr, OT_THREAD_DIRECT_WAKE_TYPE_LINK, 0, 0, kGuestKeyIndex));
        VerifyOrQuit(otThreadDirectIsWakeBurstActive(&wi.GetInstance()));
    }

    Log("---------------------------------------------------------------------------------------");
    Log("Step 4: Advance time for handshake to complete");

    nexus.AdvanceTime(kHandshakeTimeMs);

    Log("---------------------------------------------------------------------------------------");
    Log("Step 5: Verify both sides received LINKED event");

    VerifyOrQuit(wiEvents.mLinkedCount == 1);
    VerifyOrQuit(wlEvents.mLinkedCount == 1);
    VerifyOrQuit(wlEvents.mWakeReceivedCount >= 1);

    {
        const otExtAddress &wlAddr = *reinterpret_cast<const otExtAddress *>(&wl.mRadio.mExtAddress);
        const otExtAddress &wiAddr = *reinterpret_cast<const otExtAddress *>(&wi.mRadio.mExtAddress);

        VerifyOrQuit(memcmp(wiEvents.mLastPeerAddr.m8, wlAddr.m8, OT_EXT_ADDRESS_SIZE) == 0);
        VerifyOrQuit(memcmp(wlEvents.mLastPeerAddr.m8, wiAddr.m8, OT_EXT_ADDRESS_SIZE) == 0);
    }

    Log("---------------------------------------------------------------------------------------");
    Log("Step 6: WI initiates teardown");

    {
        const otExtAddress &wlAddr = *reinterpret_cast<const otExtAddress *>(&wl.mRadio.mExtAddress);

        SuccessOrQuit(otThreadDirectUnlink(&wi.GetInstance(), &wlAddr));
    }

    nexus.AdvanceTime(kTeardownTimeMs);

    Log("---------------------------------------------------------------------------------------");
    Log("Step 7: Verify both sides received UNLINKED event");

    VerifyOrQuit(wiEvents.mUnlinkedCount == 1);
    VerifyOrQuit(wlEvents.mUnlinkedCount == 1);
}

} // namespace Nexus
} // namespace ot

int main(void)
{
    setenv("OT_NEXUS_PCAP_FILE", (getenv("OT_NEXUS_PCAP_FILE_DEFAULT") ? getenv("OT_NEXUS_PCAP_FILE_DEFAULT") : ""),
           /*overwrite=*/1);
    ot::Nexus::TestTdLinkHandshake();

    setenv("OT_NEXUS_PCAP_FILE", (getenv("OT_NEXUS_PCAP_FILE_GUEST") ? getenv("OT_NEXUS_PCAP_FILE_GUEST") : ""),
           /*overwrite=*/1);
    ot::Nexus::TestTdLinkHandshakeGuestKey();

    printf("All tests passed\n");
    return 0;
}
