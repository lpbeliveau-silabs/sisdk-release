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
 *   This file implements LTV (Length-Type-Value) generation and parsing.
 */

#include "ltvs.hpp"

#include "common/code_utils.hpp"

namespace ot {

Error Ltv::AppendTo(FrameBuilder &aFrameBuilder) const { return aFrameBuilder.AppendBytes(this, GetTotalSize()); }

Error Ltv::Append(FrameBuilder &aFrameBuilder, uint8_t aType, const void *aValue, uint8_t aLength)
{
    Error error;
    Ltv   ltv;

    ltv.SetLength(aLength);
    ltv.SetType(aType);
    SuccessOrExit(error = aFrameBuilder.AppendBytes(&ltv, kHeaderSize));

    if (aLength > 0)
    {
        SuccessOrExit(error = aFrameBuilder.AppendBytes(aValue, aLength));
    }

exit:
    return error;
}

const Ltv *Ltv::FindLtv(const void *aBuffer, uint16_t aLength, uint8_t aType)
{
    Iterator   iter;
    const Ltv *result = nullptr;

    iter.Init(static_cast<const uint8_t *>(aBuffer), aLength);

    while (!iter.IsDone())
    {
        // Capture the LTV reference before advancing. Advance() validates that the
        // declared value fits in the remaining buffer; kErrorParse means truncated.
        const Ltv &ltv = iter.GetLtv();

        VerifyOrExit(iter.Advance() == kErrorNone);

        if (ltv.GetType() == aType)
        {
            result = &ltv;
            ExitNow();
        }
    }

exit:
    return result;
}

void Ltv::Iterator::Init(const uint8_t *aBuffer, uint16_t aLength) { mData.Init(aBuffer, aLength); }

Error Ltv::Iterator::Advance(void)
{
    Error    error = kErrorNone;
    uint16_t size;

    VerifyOrExit(!IsDone());

    size = GetLtv().GetTotalSize();
    VerifyOrExit(mData.CanRead(size), error = kErrorParse);
    mData.SkipOver(size);

exit:
    return error;
}

} // namespace ot
