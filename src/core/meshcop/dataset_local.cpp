/*
 *  Copyright (c) 2016-2017, The OpenThread Authors.
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
 *   This file implements common methods for manipulating MeshCoP Datasets.
 *
 */

#define WPP_NAME "dataset_local.tmh"

#include "dataset_local.hpp"

#include <stdio.h>

#include <openthread/platform/settings.h>

#include "common/code_utils.hpp"
#include "common/instance.hpp"
#include "common/logging.hpp"
#include "common/settings.hpp"
#include "meshcop/dataset.hpp"
#include "meshcop/meshcop_tlvs.hpp"
#include "thread/mle_tlvs.hpp"

namespace ot {
namespace MeshCoP {

DatasetLocal::DatasetLocal(Instance &aInstance, const Tlv::Type aType)
    : InstanceLocator(aInstance)
    , mUpdateTime(0)
    , mType(aType)
{
}

void DatasetLocal::Clear(void)
{
    otPlatSettingsDelete(&GetInstance(), GetSettingsKey(), -1);
}

otError DatasetLocal::Get(Dataset &aDataset) const
{
    DelayTimerTlv *delayTimer;
    uint32_t       elapsed;
    otError        error;

    aDataset.mLength = sizeof(aDataset.mTlvs);
    error            = otPlatSettingsGet(&GetInstance(), GetSettingsKey(), 0, aDataset.mTlvs, &aDataset.mLength);
    VerifyOrExit(error == OT_ERROR_NONE, aDataset.mLength = 0);

    delayTimer = static_cast<DelayTimerTlv *>(aDataset.Get(Tlv::kDelayTimer));
    VerifyOrExit(delayTimer);

    elapsed = TimerMilli::GetNow() - mUpdateTime;

    if (delayTimer->GetDelayTimer() > elapsed)
    {
        delayTimer->SetDelayTimer(delayTimer->GetDelayTimer() - elapsed);
    }
    else
    {
        delayTimer->SetDelayTimer(0);
    }

    aDataset.mUpdateTime = TimerMilli::GetNow();

exit:
    return error;
}

otError DatasetLocal::Get(otOperationalDataset &aDataset) const
{
    Dataset dataset(mType);
    otError error;

    memset(&aDataset, 0, sizeof(aDataset));

    dataset.mLength = sizeof(dataset.mTlvs);
    error           = otPlatSettingsGet(&GetInstance(), GetSettingsKey(), 0, dataset.mTlvs, &dataset.mLength);
    SuccessOrExit(error);

    dataset.Get(aDataset);

exit:
    return error;
}

#if OPENTHREAD_FTD

otError DatasetLocal::Set(const otOperationalDataset &aDataset)
{
    otError error = OT_ERROR_NONE;
    Dataset dataset(mType);

    error = dataset.Set(aDataset);
    SuccessOrExit(error);

    error = Set(dataset);
    SuccessOrExit(error);

exit:
    return error;
}

#endif // OPENTHREAD_FTD

otError DatasetLocal::Set(const Dataset &aDataset)
{
    Dataset dataset(aDataset);
    otError error;

    if (mType == Tlv::kActiveTimestamp)
    {
        dataset.Remove(Tlv::kPendingTimestamp);
        dataset.Remove(Tlv::kDelayTimer);
    }

    if (dataset.GetSize() == 0)
    {
        error = otPlatSettingsDelete(&GetInstance(), GetSettingsKey(), 0);
        otLogInfoMeshCoP(GetInstance(), "%s dataset deleted", mType == Tlv::kActiveTimestamp ? "Active" : "Pending");
    }
    else
    {
        error = otPlatSettingsSet(&GetInstance(), GetSettingsKey(), dataset.GetBytes(), dataset.GetSize());
        otLogInfoMeshCoP(GetInstance(), "%s dataset set", mType == Tlv::kActiveTimestamp ? "Active" : "Pending");
    }

    SuccessOrExit(error);

    mUpdateTime = TimerMilli::GetNow();

exit:
    return error;
}

uint16_t DatasetLocal::GetSettingsKey(void) const
{
    uint16_t rval;

    if (mType == Tlv::kActiveTimestamp)
    {
        rval = static_cast<uint16_t>(Settings::kKeyActiveDataset);
    }
    else
    {
        rval = static_cast<uint16_t>(Settings::kKeyPendingDataset);
    }

    return rval;
}

int DatasetLocal::Compare(const Timestamp *aCompareTimestamp)
{
    const Timestamp *thisTimestamp;
    Dataset          dataset(mType);
    int              rval = 1;

    SuccessOrExit(Get(dataset));

    thisTimestamp = dataset.GetTimestamp();

    if (aCompareTimestamp == NULL && thisTimestamp == NULL)
    {
        rval = 0;
    }
    else if (aCompareTimestamp == NULL && thisTimestamp != NULL)
    {
        rval = -1;
    }
    else if (aCompareTimestamp != NULL && thisTimestamp == NULL)
    {
        rval = 1;
    }
    else
    {
        rval = thisTimestamp->Compare(*aCompareTimestamp);
    }

exit:
    return rval;
}

} // namespace MeshCoP
} // namespace ot
