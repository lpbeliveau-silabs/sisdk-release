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
 *   This file includes definitions for generating and parsing LTV (Length-Type-Value) encoded data.
 */

#ifndef OT_CORE_COMMON_LTVS_HPP_
#define OT_CORE_COMMON_LTVS_HPP_

#include "openthread-core-config.h"

#include <stdint.h>

#include <openthread/platform/toolchain.h>

#include "common/error.hpp"
#include "common/frame_builder.hpp"
#include "common/frame_data.hpp"
#include "common/type_traits.hpp"

namespace ot {

/**
 * Implements LTV (Length-Type-Value) generation and parsing.
 *
 * The wire format is a two-byte header followed by zero or more value bytes:
 *
 *   +---------+---------+-------------------+
 *   | Length  |  Type   |  Value (0+ bytes) |
 *   +---------+---------+-------------------+
 *   | 1 byte  | 1 byte  |   Length bytes    |
 *   +---------+---------+-------------------+
 *
 * Length is the byte count of the Value only; it does not include the Type byte.
 * An LTV with Length=0 carries no Value bytes.
 *
 * The backing store for LTV sequences is raw byte buffers accessed via `FrameBuilder`
 * (write) and `FrameData` (read), not `Message` objects.
 */
OT_TOOL_PACKED_BEGIN
class Ltv
{
public:
    static constexpr uint8_t kHeaderSize = 2; ///< Wire size of the LTV header (Length byte + Type byte).

    /**
     * Returns the Length field value.
     *
     * The Length is the byte count of the Value only; it does not include the Type byte.
     *
     * @returns The Length field value.
     */
    uint8_t GetLength(void) const { return mLength; }

    /**
     * Sets the Length field value.
     *
     * @param[in] aLength  The Length value (byte count of the Value, not including the Type byte).
     */
    void SetLength(uint8_t aLength) { mLength = aLength; }

    /**
     * Returns the Type field value.
     *
     * @returns The Type field value.
     */
    uint8_t GetType(void) const { return mType; }

    /**
     * Sets the Type field value.
     *
     * @param[in] aType  The Type value.
     */
    void SetType(uint8_t aType) { mType = aType; }

    /**
     * Returns the total wire size of this LTV in bytes, including the header and the Value.
     *
     * @returns `kHeaderSize` + `GetLength()`.
     */
    uint16_t GetTotalSize(void) const { return kHeaderSize + mLength; }

    /**
     * Returns a pointer to the Value bytes.
     *
     * @returns A pointer to the first Value byte.
     */
    uint8_t *GetValue(void) { return reinterpret_cast<uint8_t *>(this) + kHeaderSize; }

    /**
     * Returns a pointer to the Value bytes.
     *
     * @returns A pointer to the first Value byte.
     */
    const uint8_t *GetValue(void) const { return reinterpret_cast<const uint8_t *>(this) + kHeaderSize; }

    /**
     * Returns a pointer to the next LTV in the sequence.
     *
     * The caller MUST ensure the next LTV lies within a valid buffer.
     *
     * @returns A pointer to the next LTV.
     */
    const Ltv *GetNext(void) const
    {
        return reinterpret_cast<const Ltv *>(reinterpret_cast<const uint8_t *>(this) + GetTotalSize());
    }

    /**
     * Appends this LTV (header and Value) to a `FrameBuilder`.
     *
     * @param[in] aFrameBuilder  The `FrameBuilder` to append to.
     *
     * @retval kErrorNone    Successfully appended.
     * @retval kErrorNoBufs  Insufficient buffer space.
     */
    Error AppendTo(FrameBuilder &aFrameBuilder) const;

    /**
     * Appends an LTV with a given type and value bytes to a `FrameBuilder`.
     *
     * @param[in] aFrameBuilder  The `FrameBuilder` to append to.
     * @param[in] aType          The LTV Type value.
     * @param[in] aValue         A pointer to the Value bytes. May be `nullptr` when @p aLength is zero.
     * @param[in] aLength        The byte count of @p aValue.
     *
     * @retval kErrorNone    Successfully appended.
     * @retval kErrorNoBufs  Insufficient buffer space.
     */
    static Error Append(FrameBuilder &aFrameBuilder, uint8_t aType, const void *aValue, uint8_t aLength);

    /**
     * Appends a fixed-size LTV using a `SimpleLtvInfo` type parameter.
     *
     * @tparam SimpleLtvType  A `SimpleLtvInfo<kType, ValueType>` specialization providing `kType` and `ValueType`.
     *
     * @param[in] aFrameBuilder  The `FrameBuilder` to append to.
     * @param[in] aValue         A const reference to the value to encode.
     *
     * @retval kErrorNone    Successfully appended.
     * @retval kErrorNoBufs  Insufficient buffer space.
     */
    template <typename SimpleLtvType>
    static Error Append(FrameBuilder &aFrameBuilder, const typename SimpleLtvType::ValueType &aValue)
    {
        static_assert(!TypeTraits::IsPointer<typename SimpleLtvType::ValueType>::kValue,
                      "SimpleLtvType::ValueType must not be a pointer");

        return Append(aFrameBuilder, SimpleLtvType::kType, &aValue, sizeof(typename SimpleLtvType::ValueType));
    }

    /**
     * Searches a raw byte buffer for the first LTV with a given type.
     *
     * Stops and returns `nullptr` on a malformed (truncated) buffer.
     *
     * @param[in] aBuffer  Pointer to the start of the LTV sequence.
     * @param[in] aLength  Byte count of the buffer.
     * @param[in] aType    The LTV Type to search for.
     *
     * @returns A pointer to the matching `Ltv` within @p aBuffer, or `nullptr` if not found.
     */
    static const Ltv *FindLtv(const void *aBuffer, uint16_t aLength, uint8_t aType);

    /**
     * Iterates forward through an LTV sequence in a raw byte buffer.
     *
     * Usage:
     * @code
     *   Ltv::Iterator iter;
     *   iter.Init(buffer, length);
     *   while (!iter.IsDone())
     *   {
     *       const Ltv &ltv = iter.GetLtv();
     *       // process ltv.GetType() and ltv.GetValue()
     *       IgnoreError(iter.Advance());
     *   }
     * @endcode
     */
    class Iterator
    {
    public:
        /**
         * Initializes the iterator over a raw byte buffer.
         *
         * @param[in] aBuffer  Pointer to the start of the LTV sequence.
         * @param[in] aLength  Byte count of the buffer.
         */
        void Init(const uint8_t *aBuffer, uint16_t aLength);

        /**
         * Indicates whether there are no more LTVs to read.
         *
         * Returns `TRUE` when the remaining buffer is too small to contain an LTV header.
         *
         * @retval TRUE   No more LTVs remain.
         * @retval FALSE  At least one more LTV is available via `GetLtv()`.
         */
        bool IsDone(void) const { return mData.GetLength() < kHeaderSize; }

        /**
         * Returns the current LTV.
         *
         * MUST NOT be called when `IsDone()` returns `TRUE`.
         *
         * @returns A const reference to the current LTV.
         */
        const Ltv &GetLtv(void) const { return *reinterpret_cast<const Ltv *>(mData.GetBytes()); }

        /**
         * Advances the iterator to the next LTV.
         *
         * When `IsDone()` is `TRUE`, this is a no-op that returns `kErrorNone`.
         *
         * @retval kErrorNone   Successfully advanced (or already done).
         * @retval kErrorParse  Buffer is truncated: remaining bytes are fewer than the current LTV declares.
         */
        Error Advance(void);

    private:
        FrameData mData;
    };

private:
    uint8_t mLength; ///< Byte count of the Value (does not include the Type byte).
    uint8_t mType;
} OT_TOOL_PACKED_END;

/**
 * Provides compile-time type information for an LTV type.
 *
 * @tparam kLtvTypeValue  The LTV Type byte value.
 */
template <uint8_t kLtvTypeValue> class LtvInfo
{
public:
    static constexpr uint8_t kType = kLtvTypeValue; ///< The LTV Type value.
};

/**
 * Provides compile-time type information for a fixed-size LTV.
 *
 * Intended for use with `Ltv::Append<SimpleLtvType>()`.
 *
 * @tparam kLtvTypeValue  The LTV Type byte value.
 * @tparam TValueType     The C++ type of the LTV's Value field.
 */
template <uint8_t kLtvTypeValue, typename TValueType> class SimpleLtvInfo : public LtvInfo<kLtvTypeValue>
{
public:
    typedef TValueType ValueType; ///< The C++ type of the Value field.
};

} // namespace ot

#endif // OT_CORE_COMMON_LTVS_HPP_
