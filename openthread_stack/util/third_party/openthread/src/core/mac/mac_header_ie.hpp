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
 *   This file includes definitions for generating and processing IEEE 802.15.4 IE (Information Element).
 */

#ifndef OT_CORE_MAC_MAC_HEADER_IE_HPP_
#define OT_CORE_MAC_MAC_HEADER_IE_HPP_

#include "openthread-core-config.h"

#include "common/as_core_type.hpp"
#include "common/bit_utils.hpp"
#include "common/encoding.hpp"
#include "common/frame_builder.hpp"
#include "common/ltvs.hpp"
#include "common/numeric_limits.hpp"
#include "mac/mac_types.hpp"

namespace ot {
namespace Mac {

/**
 * @addtogroup core-mac
 *
 * @{
 */

/**
 * Implements IEEE 802.15.4 IE (Information Element) header generation and parsing.
 */
OT_TOOL_PACKED_BEGIN
class HeaderIe
{
public:
    /**
     * Initializes the Header IE.
     */
    void Init(void) { mFields.m16 = 0; }

    /**
     * Initializes the Header IE with Id and Length.
     *
     * @param[in]  aId   The IE Element Id.
     * @param[in]  aLen  The IE content length.
     */
    void Init(uint16_t aId, uint8_t aLen);

    /**
     * Returns the IE Element Id.
     *
     * @returns the IE Element Id.
     */
    uint16_t GetId(void) const { return ReadBitsLittleEndian<uint16_t, kIdMask>(mFields.m16); }

    /**
     * Sets the IE Element Id.
     *
     * @param[in]  aId  The IE Element Id.
     */
    void SetId(uint16_t aId) { mFields.m16 = UpdateBitsLittleEndian<uint16_t, kIdMask>(mFields.m16, aId); }

    /**
     * Returns the IE content length.
     *
     * @returns the IE content length.
     */
    uint8_t GetLength(void) const { return ReadBits<uint8_t, kLengthMask>(mFields.m8[0]); }

    /**
     * Sets the IE content length.
     *
     * @param[in]  aLength  The IE content length.
     */
    void SetLength(uint8_t aLength) { WriteBits<uint8_t, kLengthMask>(mFields.m8[0], aLength); }

private:
    // Header IE format:
    //
    // +-----------+------------+--------+
    // | Bits: 0-6 |    7-14    |   15   |
    // +-----------+------------+--------+
    // | Length    | Element ID | Type=0 |
    // +-----------+------------+--------+

    static constexpr uint8_t  kSize       = 2;
    static constexpr uint8_t  kIdOffset   = 7;
    static constexpr uint8_t  kLengthMask = 0x7f;
    static constexpr uint16_t kIdMask     = 0x00ff << kIdOffset;

    union OT_TOOL_PACKED_FIELD
    {
        uint8_t  m8[kSize];
        uint16_t m16;
    } mFields;

} OT_TOOL_PACKED_END;

/**
 * Implements CSL IE data structure.
 */
OT_TOOL_PACKED_BEGIN
class CslIe
{
public:
    static constexpr uint8_t kHeaderIeId    = 0x1a;
    static constexpr uint8_t kIeContentSize = sizeof(uint16_t) * 2;

    /**
     * Returns the CSL Period.
     *
     * @returns the CSL Period.
     */
    uint16_t GetPeriod(void) const { return LittleEndian::HostSwap16(mPeriod); }

    /**
     * Sets the CSL Period.
     *
     * @param[in]  aPeriod  The CSL Period.
     */
    void SetPeriod(uint16_t aPeriod) { mPeriod = LittleEndian::HostSwap16(aPeriod); }

    /**
     * Returns the CSL Phase.
     *
     * @returns the CSL Phase.
     */
    uint16_t GetPhase(void) const { return LittleEndian::HostSwap16(mPhase); }

    /**
     * Sets the CSL Phase.
     *
     * @param[in]  aPhase  The CSL Phase.
     */
    void SetPhase(uint16_t aPhase) { mPhase = LittleEndian::HostSwap16(aPhase); }

private:
    uint16_t mPhase;
    uint16_t mPeriod;
} OT_TOOL_PACKED_END;

/**
 * Implements Termination2 IE.
 *
 * Is empty for template specialization.
 */
class Termination2Ie
{
public:
    static constexpr uint8_t kHeaderIeId    = 0x7f;
    static constexpr uint8_t kIeContentSize = 0;
};

/**
 * Implements vendor specific Header IE generation and parsing.
 */
OT_TOOL_PACKED_BEGIN
class VendorIeHeader
{
public:
    static constexpr uint8_t kHeaderIeId    = 0x00;
    static constexpr uint8_t kIeContentSize = sizeof(uint8_t) * 4;

    /**
     * Returns the Vendor OUI.
     *
     * @returns The Vendor OUI.
     */
    uint32_t GetVendorOui(void) const { return LittleEndian::ReadUint24(mOui); }

    /**
     * Sets the Vendor OUI.
     *
     * @param[in]  aVendorOui  A Vendor OUI.
     */
    void SetVendorOui(uint32_t aVendorOui) { LittleEndian::WriteUint24(aVendorOui, mOui); }

    /**
     * Returns the Vendor IE sub-type.
     *
     * @returns The Vendor IE sub-type.
     */
    uint8_t GetSubType(void) const { return mSubType; }

    /**
     * Sets the Vendor IE sub-type.
     *
     * @param[in]  aSubType  The Vendor IE sub-type.
     */
    void SetSubType(uint8_t aSubType) { mSubType = aSubType; }

private:
    static constexpr uint8_t kOuiSize = 3;

    uint8_t mOui[kOuiSize];
    uint8_t mSubType;
} OT_TOOL_PACKED_END;

#if OPENTHREAD_CONFIG_TIME_SYNC_ENABLE
/**
 * Implements Time Header IE generation and parsing.
 */
OT_TOOL_PACKED_BEGIN
class TimeIe : public VendorIeHeader
{
public:
    static constexpr uint32_t kVendorOuiNest = 0x18b430;
    static constexpr uint8_t  kVendorIeTime  = 0x01;
    static constexpr uint8_t  kHeaderIeId    = VendorIeHeader::kHeaderIeId;
    static constexpr uint8_t  kIeContentSize = VendorIeHeader::kIeContentSize + sizeof(uint8_t) + sizeof(uint64_t);

    /**
     * Initializes the time IE.
     */
    void Init(void)
    {
        SetVendorOui(kVendorOuiNest);
        SetSubType(kVendorIeTime);
    }

    /**
     * Returns the time sync sequence.
     *
     * @returns the time sync sequence.
     */
    uint8_t GetSequence(void) const { return mSequence; }

    /**
     * Sets the tine sync sequence.
     *
     * @param[in]  aSequence The time sync sequence.
     */
    void SetSequence(uint8_t aSequence) { mSequence = aSequence; }

    /**
     * Returns the network time.
     *
     * @returns the network time, in microseconds.
     */
    uint64_t GetTime(void) const { return LittleEndian::HostSwap64(mTime); }

    /**
     * Sets the network time.
     *
     * @param[in]  aTime  The network time.
     */
    void SetTime(uint64_t aTime) { mTime = LittleEndian::HostSwap64(aTime); }

private:
    uint8_t  mSequence;
    uint64_t mTime;
} OT_TOOL_PACKED_END;
#endif // OPENTHREAD_CONFIG_TIME_SYNC_ENABLE

class ThreadIe
{
public:
    static constexpr uint8_t  kHeaderIeId               = VendorIeHeader::kHeaderIeId;
    static constexpr uint8_t  kIeContentSize            = VendorIeHeader::kIeContentSize;
    static constexpr uint32_t kVendorOuiThreadCompanyId = 0xeab89b;
    static constexpr uint8_t  kEnhAckProbingIe          = 0x00;
};

#if OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE
/**
 * Defines the Thread Header IE Element ID and LTV type codes.
 *
 * The Thread Header IE is an IEEE 802.15.4 Header IE with Element ID 0x2d.
 * Its payload is a sequence of LTV (Length-Type-Value) encoded elements.
 */
struct ThreadHeaderIe
{
    static constexpr uint8_t kElementId     = 0x2d; ///< IEEE 802.15.4 Header IE Element ID for Thread Group.
    static constexpr uint8_t kTypeTargetId  = 0x01; ///< Target ID LTV - Wake Identifier filter.
    static constexpr uint8_t kTypeSca       = 0x02; ///< Scheduled Channel Access LTV.
    static constexpr uint8_t kTypeChallenge = 0x03; ///< Thread Challenge LTV.

    // Challenge LTV (18 B) + SCA LTV with full 256-bit RAM bitmap and SLW (41 B) = 59 B.
    // Use 64 to stay aligned and leave one byte of margin.
    static constexpr uint8_t kEnhAckPlainMaxSize = 64;
};

/**
 * Enumerates SCA slot duration encodings from the current Thread Direct specification.
 */
enum class ScaSlotDuration : uint8_t
{
    k625Usec  = 0, ///< 625 us slot duration.
    k1250Usec = 1, ///< 1.25 ms slot duration.
    k625Msec  = 2, ///< 625 ms slot duration.
    k1250Msec = 3, ///< 1.25 s slot duration.
};

/**
 * In-memory representation of SCA LTV parameters.
 *
 * Wire format (value bytes after Type):
 *   2B fixed header (LE): bits[1:0]=SlotDuration, bits[12:2]=RamOffset, bit[13]=RamAvailable, bits[15:14]=RSV
 *   If mRamAvailable: 1B RamDuration + ceil(mRamDuration/8) RAM Bits bytes
 *   If mHasSlw: 2B SLW Period (LE) + 2B SLW Phase (LE)
 *   Teardown: zero-length value (mIsTeardown flag; AppendScaLtvTeardown emits this form).
 *
 * SLW Period and SLW Phase are expressed in units of Slot Duration.
 *
 * RAM Available = FALSE indicates no CoEx constraints and no RAM bitmap.
 * RAM Available = TRUE with RAM Duration = 0 indicates no change to the
 * previously communicated RAM, while SLW fields may still be updated.
 */
struct ScaParams
{
    static constexpr uint8_t kRamBitsMaxBytes = 32; ///< Maximum RAM bitmap size (256 bits).
    static constexpr int16_t kRamOffsetUsMin  = -1024;
    static constexpr int16_t kRamOffsetUsMax  = 1023;

    uint16_t        mSlwPeriodSlots; ///< SLW period in slot-duration units (0 = rx-on-when-idle); valid when mHasSlw.
    uint16_t        mSlwPhaseSlots;  ///< SLW phase in slot-duration units; valid when mHasSlw.
    int16_t         mRamOffsetUs;    ///< RAM Offset in us, signed [-1024, 1023].
    uint8_t         mRamDuration;    ///< Number of bits in mRamBits (0 = no change); valid when mRamAvailable.
    ScaSlotDuration mSlotDuration;   ///< Slot duration code: 0=625us, 1=1.25ms, 2=625ms, 3=1.25s.
    bool            mRamAvailable;   ///< True if RAM Duration and RAM Bits are present in the SCA LTV.
    bool            mHasSlw;         ///< True if SLW Period and Phase are present in the SCA LTV.
    uint8_t         mRamBits[kRamBitsMaxBytes]; ///< RAM bitmap; ceil(mRamDuration/8) bytes valid when mRamAvailable.
};

/**
 * Holds the 16-byte challenge value carried in a Thread Direct Challenge LTV.
 */
struct ChallengeLtv
{
    static constexpr uint8_t kLength = 16; ///< Challenge value length in bytes (truncated HMAC-SHA256).
    uint8_t                  mChallenge[kLength];
};

/**
 * Appends a Thread Header IE (Element ID 0x2d) containing the given LTV payload to a FrameBuilder.
 *
 * @param[in,out] aFrameBuilder  The FrameBuilder to append to.
 * @param[in]     aLtvPayload    Pointer to the serialised LTV bytes to place inside the IE.
 * @param[in]     aLtvLen        Length of aLtvPayload in bytes (must fit in 7-bit IE length field).
 *
 * @retval kErrorNone   Successfully appended.
 * @retval kErrorNoBufs Insufficient space in the FrameBuilder.
 */
Error AppendThreadHeaderIe(FrameBuilder &aFrameBuilder, const uint8_t *aLtvPayload, uint8_t aLtvLen);

/**
 * Appends an SCA LTV to a FrameBuilder.
 *
 * @param[in,out] aFrameBuilder  The FrameBuilder to append to.
 * @param[in]     aParams        SCA parameters to encode.
 *
 * @retval kErrorNone   Successfully appended.
 * @retval kErrorNoBufs Insufficient space in the FrameBuilder.
 */
Error AppendScaLtv(FrameBuilder &aFrameBuilder, const ScaParams &aParams);

/**
 * Appends a zero-length (teardown) SCA LTV to a FrameBuilder.
 *
 * A zero-length SCA LTV signals TD link teardown to the receiver.
 *
 * @param[in,out] aFrameBuilder  The FrameBuilder to append to.
 *
 * @retval kErrorNone   Successfully appended.
 * @retval kErrorNoBufs Insufficient space in the FrameBuilder.
 */
Error AppendScaLtvTeardown(FrameBuilder &aFrameBuilder);

/**
 * Appends a Challenge LTV to a FrameBuilder.
 *
 * @param[in,out] aFrameBuilder  The FrameBuilder to append to.
 * @param[in]     aChallenge     The 16-byte challenge value to encode.
 *
 * @retval kErrorNone   Successfully appended.
 * @retval kErrorNoBufs Insufficient space in the FrameBuilder.
 */
Error AppendChallengeLtv(FrameBuilder &aFrameBuilder, const ChallengeLtv &aChallenge);

/**
 * Appends a Target ID LTV to a FrameBuilder.
 *
 * @param[in,out] aFrameBuilder  The FrameBuilder to append to.
 * @param[in]     aTargetId      Pointer to the Target ID bytes.
 * @param[in]     aLen           Length of aTargetId in bytes.
 *
 * @retval kErrorNone   Successfully appended.
 * @retval kErrorNoBufs Insufficient space in the FrameBuilder.
 */
Error AppendTargetIdLtv(FrameBuilder &aFrameBuilder, const uint8_t *aTargetId, uint8_t aLen);

/**
 * Packs a sequence of plain Length-Type-Value entries (as produced by `Ltv::Append`) into the
 * Thread Header IE adaptive packing format defined in Thread Specification chapter 16.
 *
 * The number of bits used for length and type in the first header byte is determined by the
 * total remaining byte count L at each LTV's position in the sequence.  Packing always produces
 * output that is equal in size or smaller than the plain input.
 *
 * @param[in]  aPlain     Pointer to the plain LTV bytes (each entry: 1-byte Length, 1-byte Type,
 *                        Length value bytes).
 * @param[in]  aPlainLen  Length of @p aPlain in bytes.
 * @param[out] aPacked    Output buffer.  Must be at least @p aPlainLen bytes.
 * @param[in]  aPackedMax Size of @p aPacked.
 *
 * @returns  Number of bytes written to @p aPacked.  Zero if @p aPlainLen is zero.
 */
uint8_t PackThreadHeaderIeLtvs(const uint8_t *aPlain, uint8_t aPlainLen, uint8_t *aPacked, uint8_t aPackedMax);

/**
 * Unpacks a Thread Header IE adaptive-packing LTV sequence into plain Length-Type-Value form.
 *
 * @param[in]  aPacked     Pointer to the packed LTV bytes.
 * @param[in]  aPackedLen  Length of @p aPacked in bytes.
 * @param[out] aPlain      Output buffer for the plain LTV bytes.
 * @param[in]  aPlainMax   Size of @p aPlain.
 * @param[out] aPlainLen   Set to the number of plain bytes written on success.
 *
 * @retval kErrorNone    Successfully unpacked.
 * @retval kErrorParse   Buffer is truncated or malformed.
 * @retval kErrorNoBufs  @p aPlain is too small for the unpacked output.
 */
Error UnpackThreadHeaderIeLtvs(const uint8_t *aPacked,
                               uint8_t        aPackedLen,
                               uint8_t       *aPlain,
                               uint8_t        aPlainMax,
                               uint8_t       &aPlainLen);

/**
 * Parses the LTV payload of a Thread Header IE.
 *
 * Unpacks the adaptive-packed LTV sequence, then walks all entries.  For each LTV whose type
 * matches a known Thread Direct type, the output parameter is populated when non-null.  Unknown
 * types are skipped.  A zero-length SCA LTV (teardown signal) sets @p aTeardown when non-null.
 *
 * @param[in]  aBuffer           Pointer to the IE payload (the bytes after the 2-byte HeaderIe header).
 * @param[in]  aLength           Length of @p aBuffer in bytes.
 * @param[out] aScaParams        If non-null, populated when a non-empty SCA LTV is found.
 * @param[out] aChallenge        If non-null, populated when a Challenge LTV is found.
 * @param[out] aTeardown         If non-null, set to true when a zero-length (teardown) SCA LTV is found.
 * @param[out] aChallengePresent If non-null, set to true when a Challenge LTV is found.
 *
 * @retval kErrorNone   Parsing succeeded (all LTV entries are well-formed).
 * @retval kErrorParse  Buffer is truncated or malformed.
 */
Error ParseThreadHeaderIe(const uint8_t *aBuffer,
                          uint8_t        aLength,
                          ScaParams     *aScaParams,
                          ChallengeLtv  *aChallenge,
                          bool          *aTeardown         = nullptr,
                          bool          *aChallengePresent = nullptr);

#endif // OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_INITIATOR_ENABLE || OPENTHREAD_CONFIG_THREAD_DIRECT_WAKE_LISTENER_ENABLE

/**
 * @}
 */

} // namespace Mac
} // namespace ot

#endif // OT_CORE_MAC_MAC_HEADER_IE_HPP_
