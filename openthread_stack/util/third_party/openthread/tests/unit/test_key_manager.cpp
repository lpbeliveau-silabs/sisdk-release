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

#include <string.h>

#include "common/code_utils.hpp"
#include "common/debug.hpp"
#include "thread/key_manager.hpp"

#include "test_platform.h"
#include "test_util.hpp"

namespace ot {

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

void TestWakeKeyDerivation(void)
{
    // `KeyManager::GetDefaultWakeKey()` matches the first 16 bytes of
    // HMAC-SHA256 over the literal "Thread-Wake" using the Thread Network Key
    // as the HMAC key (see `KeyManager::GetDefaultWakeKey()`).
    //
    // Test vector (network key is non-zero on purpose):
    //   Network Key : 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
    //   Wake Key    : f5 ca 7d 94 57 48 c4 55 e8 ad 69 6a 76 fb 18 d0
    //
    // Recompute with:
    //   python3 -c "import
    //   hmac,hashlib;k=bytes(range(16));print(hmac.new(k,b'Thread-Wake',hashlib.sha256).digest()[:16].hex())"

    constexpr uint8_t kNetworkKeyBytes[OT_NETWORK_KEY_SIZE] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };

    constexpr uint8_t kExpectedWakeKey[Mac::Key::kSize] = {
        0xf5, 0xca, 0x7d, 0x94, 0x57, 0x48, 0xc4, 0x55, 0xe8, 0xad, 0x69, 0x6a, 0x76, 0xfb, 0x18, 0xd0,
    };

    Instance  *instance;
    NetworkKey networkKey;
    Mac::Key   derivedKey;

    instance = testInitInstance();
    VerifyOrQuit(instance != nullptr);

    memcpy(networkKey.m8, kNetworkKeyBytes, sizeof(networkKey.m8));
    instance->Get<KeyManager>().SetNetworkKey(networkKey);

    const Mac::KeyMaterial &wakeMaterial = instance->Get<KeyManager>().GetDefaultWakeKey();

    wakeMaterial.ExtractKey(derivedKey);
    VerifyOrQuit(memcmp(derivedKey.GetBytes(), kExpectedWakeKey, Mac::Key::kSize) == 0, "Wake key derivation mismatch");

    testFreeInstance(instance);
}

void TestChallengeComputation(void)
{
    // Challenge = HMAC-SHA256(WakeKey, LinkFC || WakeFC || WakeID || LinkSeq)[0:15]
    //
    // WakeKey for index 129: HMAC-SHA256(NetworkKey, "Thread-Wake")[0:15]
    //
    // Test vectors computed with:
    //   python3 -c "
    //   import hmac, hashlib
    //   net_key = bytes([0x00,0x11,...,0xFF])
    //   wake_key = hmac.new(net_key, b'Thread-Wake', hashlib.sha256).digest()[:16]
    //   msg = link_fc.to_bytes(4,'big') + wake_fc.to_bytes(4,'big') + bytes(8) + bytes([link_seq])
    //   print(hmac.new(wake_key, msg, hashlib.sha256).digest()[:16].hex())
    //   "

    // --- Vector 1: default wake key (index 129), specific counters ---
    //
    // Network Key : 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF
    // Wake Key    : 76 B7 53 8C CD 02 00 E6 AE 77 A9 77 38 AC 96 A2
    // LinkFC      : 0x00000042
    // WakeFC      : 0x00000100
    // WakeID      : 00 00 00 00 00 00 00 00 (8 zero bytes)
    // LinkSeq     : 1
    // Challenge   : 3D 13 2F EE 30 68 03 E3 27 87 1D BF 60 70 FF EF
    {
        constexpr uint8_t kNetKey[OT_NETWORK_KEY_SIZE] = {
            0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
        };
        constexpr uint8_t kExpected[Mac::ChallengeLtv::kLength] = {
            0x3D, 0x13, 0x2F, 0xEE, 0x30, 0x68, 0x03, 0xE3, 0x27, 0x87, 0x1D, 0xBF, 0x60, 0x70, 0xFF, 0xEF,
        };

        Instance         *instance = testInitInstance();
        NetworkKey        networkKey;
        Mac::ChallengeLtv challenge;

        VerifyOrQuit(instance != nullptr);

        memcpy(networkKey.m8, kNetKey, sizeof(networkKey.m8));
        instance->Get<KeyManager>().SetNetworkKey(networkKey);

        VerifyOrQuit(instance->Get<KeyManager>().ComputeChallenge(Mac::Frame::kWakeKeyIndex, 0x00000042, 0x00000100,
                                                                  nullptr, 1, challenge) == kErrorNone,
                     "ComputeChallenge failed for default key");
        VerifyOrQuit(memcmp(challenge.mChallenge, kExpected, Mac::ChallengeLtv::kLength) == 0,
                     "Challenge mismatch for vector 1");

        testFreeInstance(instance);
    }

    // --- Vector 2: default wake key, all-zeros network key, counters 1/2, seq 0 ---
    //
    // Network Key : 00 00 ... 00
    // Wake Key    : D9 43 9C 1E DE 81 AA FD 1C 8E A4 53 D5 A8 C9 28
    // LinkFC      : 0x00000001, WakeFC: 0x00000002, LinkSeq: 0
    // Challenge   : B8 30 80 32 6C FF EC 1E 2C 45 AC 06 20 DF C1 C7
    {
        constexpr uint8_t kExpected2[Mac::ChallengeLtv::kLength] = {
            0xB8, 0x30, 0x80, 0x32, 0x6C, 0xFF, 0xEC, 0x1E, 0x2C, 0x45, 0xAC, 0x06, 0x20, 0xDF, 0xC1, 0xC7,
        };

        Instance         *instance = testInitInstance();
        NetworkKey        networkKey;
        Mac::ChallengeLtv challenge;

        VerifyOrQuit(instance != nullptr);
        memset(networkKey.m8, 0, sizeof(networkKey.m8));
        instance->Get<KeyManager>().SetNetworkKey(networkKey);

        VerifyOrQuit(instance->Get<KeyManager>().ComputeChallenge(Mac::Frame::kWakeKeyIndex, 0x00000001, 0x00000002,
                                                                  nullptr, 0, challenge) == kErrorNone,
                     "ComputeChallenge failed for vector 2");
        VerifyOrQuit(memcmp(challenge.mChallenge, kExpected2, Mac::ChallengeLtv::kLength) == 0,
                     "Challenge mismatch for vector 2");

        testFreeInstance(instance);
    }

    // --- Vector 3: guest key (index 130), direct raw key, counters 1/2, seq 5 ---
    //
    // Guest Key   : DE AD BE EF CA FE BA BE 01 02 03 04 05 06 07 08
    // LinkFC      : 0x00000001, WakeFC: 0x00000002, LinkSeq: 5
    // Challenge   : F3 52 F9 5C 3A 6F C8 1C FC B0 76 50 0F EF B3 A8
    {
        constexpr uint8_t kGuestKey[Mac::Key::kSize] = {
            0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        };
        constexpr uint8_t kExpected3[Mac::ChallengeLtv::kLength] = {
            0xF3, 0x52, 0xF9, 0x5C, 0x3A, 0x6F, 0xC8, 0x1C, 0xFC, 0xB0, 0x76, 0x50, 0x0F, 0xEF, 0xB3, 0xA8,
        };

        Instance         *instance = testInitInstance();
        Mac::Key          rawKey;
        Mac::KeyMaterial  keyMaterial;
        Mac::ChallengeLtv challenge;

        VerifyOrQuit(instance != nullptr);
        memcpy(rawKey.m8, kGuestKey, sizeof(rawKey.m8));
        keyMaterial.SetFrom(rawKey, false);

        VerifyOrQuit(instance->Get<KeyManager>().SetGuestWakeKey(130, &keyMaterial) == kErrorNone,
                     "SetGuestWakeKey failed");

        VerifyOrQuit(instance->Get<KeyManager>().ComputeChallenge(130, 0x00000001, 0x00000002, nullptr, 5, challenge) ==
                         kErrorNone,
                     "ComputeChallenge failed for guest key");
        VerifyOrQuit(memcmp(challenge.mChallenge, kExpected3, Mac::ChallengeLtv::kLength) == 0,
                     "Challenge mismatch for vector 3 (guest key)");

        testFreeInstance(instance);
    }

    // --- Error case: guest key index not provisioned ---
    {
        Instance         *instance = testInitInstance();
        Mac::ChallengeLtv challenge;

        VerifyOrQuit(instance != nullptr);
        VerifyOrQuit(instance->Get<KeyManager>().ComputeChallenge(131, 0, 0, nullptr, 0, challenge) == kErrorNotFound,
                     "Expected kErrorNotFound for missing guest key");

        testFreeInstance(instance);
    }
}

#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

} // namespace ot

int main(void)
{
#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
    ot::TestWakeKeyDerivation();
    ot::TestChallengeComputation();
#endif
    printf("All tests passed\n");
    return 0;
}
