/*
 *  Copyright (c) 2016-2024, The OpenThread Authors.
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
 *   This file implements IEEE 802.15.4 header IE generation and parsing.
 */

#include "mac_header_ie.hpp"

#include "common/code_utils.hpp"
#include "common/encoding.hpp"
#include "common/frame_builder.hpp"
#include "common/frame_data.hpp"
#include "common/ltvs.hpp"

namespace ot {
namespace Mac {

void HeaderIe::Init(uint16_t aId, uint8_t aLen)
{
    Init();
    SetId(aId);
    SetLength(aLen);
}

#if (OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE) && \
    (OPENTHREAD_FTD || OPENTHREAD_MTD)

// SCA LTV fixed-header word (2-byte LE) bit positions and masks.
enum : uint16_t
{
    kScaSlotDurationShift = 0,
    kScaSlotDurationMask  = 0x0003u,
    kScaRamOffsetShift    = 2,
    kScaRamOffsetMask     = 0x07FFu,
    kScaRamOffsetMsb      = 0x0400u, ///< Sign bit of the 11-bit offset field.
    kScaRamOffsetSignExt  = 0xF800u, ///< Upper bits set when sign-extending to int16_t.
    kScaRamAvailableShift = 13,
};

// Returns ceil(log2(L)), where ceil_log2(0) == 0 and ceil_log2(1) == 0.
static uint8_t CeilLog2(uint8_t L)
{
    uint8_t n = 0;
    uint8_t p = 1;

    while (p < L)
    {
        n++;
        p = static_cast<uint8_t>(p << 1);
    }

    return n;
}

uint8_t PackThreadHeaderIeLtvs(const uint8_t *aPlain, uint8_t aPlainLen, uint8_t *aPacked, uint8_t aPackedMax)
{
    // Step 1 — collect (type, valueLen, valuePtr) from the plain L-T-V buffer.
    static constexpr uint8_t kMaxEntries = 8;

    struct Entry
    {
        uint8_t        type;
        uint8_t        valueLen;
        const uint8_t *value;
    };

    Entry   entries[kMaxEntries];
    uint8_t count = 0;

    for (const uint8_t *p = aPlain, *end = aPlain + aPlainLen; p + 2 <= end && count < kMaxEntries;)
    {
        entries[count].type     = p[1];
        entries[count].valueLen = p[0];
        entries[count].value    = p + 2;
        count++;
        p += 2 + p[0];
    }

    // Step 2 — encode back-to-front; each LTV's header bits depend on L = bytes
    // remaining from this LTV to the end of the buffer (own size + suffix size).
    uint8_t scratch[64]; // packed output is always <= plain input length
    uint8_t scratchLen = 0;

    for (int i = static_cast<int>(count) - 1; i >= 0; i--)
    {
        uint8_t        t         = entries[i].type;
        uint8_t        vLen      = entries[i].valueLen;
        const uint8_t *v         = entries[i].value;
        uint8_t        hdrBytes  = 0;
        uint8_t        firstByte = 0;
        uint8_t        typeByte  = 0;
        bool           done      = false;

        // Try compact (1-byte header) then base (2-byte header).
        for (uint8_t hdr = 1; hdr <= 2 && !done; hdr++)
        {
            uint8_t own   = hdr + vLen;
            uint8_t L     = own + scratchLen;
            uint8_t n     = CeilLog2(L);
            uint8_t shift = static_cast<uint8_t>(8 - n);

            if (n == 0)
            {
                // L == 1: encode as a lone type byte (vLen must be 0).
                if (hdr == 1 && vLen == 0)
                {
                    hdrBytes  = 1;
                    firstByte = t;
                    done      = true;
                }

                continue;
            }

            if (vLen > static_cast<uint8_t>((1u << n) - 1u))
            {
                continue;
            }

            uint8_t allOnes = static_cast<uint8_t>((1u << shift) - 1u);

            if (hdr == 1)
            {
                // Compact: type in the lower `shift` bits; all-ones is the escape.
                if (t < allOnes)
                {
                    hdrBytes  = 1;
                    firstByte = static_cast<uint8_t>((vLen << shift) | t);
                    done      = true;
                }
            }
            else
            {
                // Base: all-ones escape in lower bits; dedicated type byte follows.
                hdrBytes  = 2;
                firstByte = static_cast<uint8_t>((vLen << shift) | allOnes);
                typeByte  = t;
                done      = true;
            }
        }

        if (!done)
        {
            continue; // should not happen for valid Thread Header IE LTVs
        }

        uint8_t insertLen = hdrBytes + vLen;

        memmove(scratch + insertLen, scratch, scratchLen);
        scratch[0] = firstByte;

        if (hdrBytes == 2)
        {
            scratch[1] = typeByte;
        }

        if (vLen > 0)
        {
            memcpy(scratch + hdrBytes, v, vLen);
        }

        scratchLen += insertLen;
    }

    uint8_t written = (scratchLen <= aPackedMax) ? scratchLen : aPackedMax;
    memcpy(aPacked, scratch, written);

    return written;
}

Error UnpackThreadHeaderIeLtvs(const uint8_t *aPacked,
                               uint8_t        aPackedLen,
                               uint8_t       *aPlain,
                               uint8_t        aPlainMax,
                               uint8_t       &aPlainLen)
{
    Error   error  = kErrorNone;
    uint8_t offset = 0;
    uint8_t outLen = 0;

    while (offset < aPackedLen)
    {
        // L is the remaining byte count at this position — the same value the encoder
        // used to determine the header bit-split for this LTV.
        uint8_t L     = aPackedLen - offset;
        uint8_t n     = CeilLog2(L);
        uint8_t first = aPacked[offset];
        uint8_t ltvLen;
        uint8_t ltvType;
        uint8_t hdrSize;

        if (n == 0)
        {
            // L == 1: lone type byte, zero-length value.
            ltvType = first;
            ltvLen  = 0;
            hdrSize = 1;
        }
        else
        {
            uint8_t shift   = static_cast<uint8_t>(8 - n);
            uint8_t lenbits = first >> shift;
            uint8_t rest    = first & static_cast<uint8_t>((1u << shift) - 1u);
            uint8_t allOnes = static_cast<uint8_t>((1u << shift) - 1u);

            if (rest == allOnes)
            {
                // Base format: all-ones escape; dedicated type byte follows.
                VerifyOrExit(offset + 1u < aPackedLen, error = kErrorParse);
                ltvType = aPacked[offset + 1];
                ltvLen  = lenbits;
                hdrSize = 2;
            }
            else
            {
                ltvType = rest;
                ltvLen  = lenbits;
                hdrSize = 1;
            }
        }

        VerifyOrExit(offset + hdrSize + ltvLen <= aPackedLen, error = kErrorParse);
        VerifyOrExit(outLen + 2 + ltvLen <= aPlainMax, error = kErrorNoBufs);

        aPlain[outLen++] = ltvLen;
        aPlain[outLen++] = ltvType;

        if (ltvLen > 0)
        {
            memcpy(aPlain + outLen, aPacked + offset + hdrSize, ltvLen);
            outLen += ltvLen;
        }

        offset += hdrSize + ltvLen;
    }

    aPlainLen = outLen;

exit:
    return error;
}

Error AppendThreadHeaderIe(FrameBuilder &aFrameBuilder, const uint8_t *aLtvPayload, uint8_t aLtvLen)
{
    Error    error;
    HeaderIe ie;

    ie.Init(ThreadHeaderIe::kElementId, aLtvLen);
    SuccessOrExit(error = aFrameBuilder.AppendBytes(&ie, sizeof(ie)));

    if (aLtvLen > 0)
    {
        SuccessOrExit(error = aFrameBuilder.AppendBytes(aLtvPayload, aLtvLen));
    }

exit:
    return error;
}

Error AppendScaLtv(FrameBuilder &aFrameBuilder, const ScaParams &aParams)
{
    uint16_t fixedHdr =
        static_cast<uint16_t>((static_cast<uint16_t>(aParams.mSlotDuration) & kScaSlotDurationMask)
                              << kScaSlotDurationShift) |
        static_cast<uint16_t>((static_cast<uint16_t>(aParams.mRamOffsetUs) & kScaRamOffsetMask) << kScaRamOffsetShift) |
        (aParams.mRamAvailable ? static_cast<uint16_t>(1u << kScaRamAvailableShift) : 0u);

    uint8_t ramBitsLen = (aParams.mRamAvailable && aParams.mRamDuration > 0)
                             ? static_cast<uint8_t>((aParams.mRamDuration + 7u) / 8u)
                             : 0u;

    uint8_t valueLen =
        static_cast<uint8_t>(2u + (aParams.mRamAvailable ? (1u + ramBitsLen) : 0u) + (aParams.mHasSlw ? 4u : 0u));

    Error   error;
    uint8_t value[39]; // 2 (fixed hdr) + 1 (RAM Duration) + 32 (RAM Bits) + 4 (SLW Period + Phase)
    uint8_t pos = 0;

    LittleEndian::WriteUint16(fixedHdr, value + pos);
    pos += 2;

    if (aParams.mRamAvailable)
    {
        value[pos++] = aParams.mRamDuration;

        for (uint8_t i = 0; i < ramBitsLen; i++)
        {
            value[pos++] = aParams.mRamBits[i];
        }
    }

    if (aParams.mHasSlw)
    {
        LittleEndian::WriteUint16(aParams.mSlwPeriodSlots, value + pos);
        pos += 2;
        LittleEndian::WriteUint16(aParams.mSlwPhaseSlots, value + pos);
        pos += 2;
    }

    error = Ltv::Append(aFrameBuilder, ThreadHeaderIe::kTypeSca, value, valueLen);

    return error;
}

Error AppendScaLtvTeardown(FrameBuilder &aFrameBuilder)
{
    return Ltv::Append(aFrameBuilder, ThreadHeaderIe::kTypeSca, nullptr, 0);
}

Error AppendChallengeLtv(FrameBuilder &aFrameBuilder, const ChallengeLtv &aChallenge)
{
    return Ltv::Append(aFrameBuilder, ThreadHeaderIe::kTypeChallenge, aChallenge.mChallenge, ChallengeLtv::kLength);
}

Error AppendTargetIdLtv(FrameBuilder &aFrameBuilder, const uint8_t *aTargetId, uint8_t aLen)
{
    return Ltv::Append(aFrameBuilder, ThreadHeaderIe::kTypeTargetId, aTargetId, aLen);
}

Error ParseThreadHeaderIe(const uint8_t *aBuffer,
                          uint8_t        aLength,
                          ScaParams     *aScaParams,
                          ChallengeLtv  *aChallenge,
                          bool          *aTeardown,
                          bool          *aChallengePresent)
{
    Error   error = kErrorNone;
    uint8_t plain[64];
    uint8_t plainLen = 0;

    SuccessOrExit(error = UnpackThreadHeaderIeLtvs(aBuffer, aLength, plain, sizeof(plain), plainLen));

    {
        Ltv::Iterator iter;
        iter.Init(plain, plainLen);

        while (!iter.IsDone())
        {
            const Ltv &ltv     = iter.GetLtv();
            uint8_t    ltvType = ltv.GetType();
            uint8_t    ltvLen  = ltv.GetLength();

            VerifyOrExit(iter.Advance() == kErrorNone, error = kErrorParse);

            if (ltvType == ThreadHeaderIe::kTypeSca)
            {
                if (ltvLen == 0)
                {
                    if (aTeardown != nullptr)
                    {
                        *aTeardown = true;
                    }
                }
                else if (ltvLen >= 2 && aScaParams != nullptr)
                {
                    const uint8_t *v        = ltv.GetValue();
                    uint16_t       fixedHdr = LittleEndian::ReadUint16(v);
                    uint16_t       rawOff   = (fixedHdr >> kScaRamOffsetShift) & kScaRamOffsetMask;
                    uint8_t        consumed = 2;

                    memset(aScaParams, 0, sizeof(ScaParams));
                    aScaParams->mSlotDuration =
                        static_cast<ScaSlotDuration>((fixedHdr >> kScaSlotDurationShift) & kScaSlotDurationMask);
                    aScaParams->mRamOffsetUs  = (rawOff & kScaRamOffsetMsb)
                                                    ? static_cast<int16_t>(rawOff | kScaRamOffsetSignExt)
                                                    : static_cast<int16_t>(rawOff);
                    aScaParams->mRamAvailable = ((fixedHdr >> kScaRamAvailableShift) & 0x01u) != 0;

                    if (aScaParams->mRamAvailable && consumed < ltvLen)
                    {
                        uint8_t ramDur           = v[consumed++];
                        aScaParams->mRamDuration = ramDur;
                        uint8_t ramBitsLen       = (ramDur > 0) ? static_cast<uint8_t>((ramDur + 7u) / 8u) : 0u;

                        for (uint8_t i = 0; i < ramBitsLen && consumed < ltvLen && i < ScaParams::kRamBitsMaxBytes;
                             i++, consumed++)
                        {
                            aScaParams->mRamBits[i] = v[consumed];
                        }
                    }

                    if (static_cast<uint8_t>(ltvLen - consumed) >= 4)
                    {
                        aScaParams->mSlwPeriodSlots = LittleEndian::ReadUint16(v + consumed);
                        aScaParams->mSlwPhaseSlots  = LittleEndian::ReadUint16(v + consumed + 2);
                        aScaParams->mHasSlw         = true;
                    }
                }
            }
            else if (ltvType == ThreadHeaderIe::kTypeChallenge && aChallenge != nullptr &&
                     ltvLen == ChallengeLtv::kLength)
            {
                memcpy(aChallenge->mChallenge, ltv.GetValue(), ChallengeLtv::kLength);

                if (aChallengePresent != nullptr)
                {
                    *aChallengePresent = true;
                }
            }
        }
    }

exit:
    return error;
}

#endif // (OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE ||
       // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE) && (OPENTHREAD_FTD || OPENTHREAD_MTD)

} // namespace Mac
} // namespace ot
