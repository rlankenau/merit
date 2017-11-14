// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2017 The Merit Foundation developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "refmempool.h"

#include "consensus/validation.h"
#include "validation.h"
#include "policy/policy.h"
#include "policy/fees.h"
#include "reverse_iterator.h"
#include "streams.h"

#include <algorithm>
#include <numeric>

namespace referral
{
RefMemPoolEntry::RefMemPoolEntry(const Referral& _entry, int64_t _nTime, unsigned int _entryHeight) : MemPoolEntry(_entry, _nTime, _entryHeight)
{
    nWeight = GetReferralWeight(_entry);
    nUsageSize = sizeof(RefMemPoolEntry);
}

size_t RefMemPoolEntry::GetSize() const
{
    return GetVirtualReferralSize(nWeight);
}

bool ReferralTxMemPool::AddUnchecked(const uint256& hash, const RefMemPoolEntry& entry)
{
    LOCK(cs);

    if (mapRTx.count(hash) == 0) {
        refiter newit = mapRTx.insert(entry).first;
        mapLinks.insert(std::make_pair(newit, RefLinks()));
    }

    nReferralsUpdated++;

    return true;
}

// Calculates descendants of entry that are not already in setDescendants, and adds to
// setDescendants. Assumes entryit is already a tx in the mempool and setMemPoolChildren
// is correct for tx and all descendants.
// Also assumes that if an entry is in setDescendants already, then all
// in-mempool descendants of it are already in setDescendants as well, so that we
// can save time by not iterating over those entries.
void ReferralTxMemPool::CalculateDescendants(refiter entryit, setEntries& setDescendants)
{
    setEntries stage;
    if (setDescendants.count(entryit) == 0) {
        stage.insert(entryit);
    }
    // Traverse down the children of entry, only adding children that are not
    // accounted for in setDescendants already (because those children have either
    // already been walked, or will be walked in this iteration).
    while (!stage.empty()) {
        refiter it = *stage.begin();
        setDescendants.insert(it);
        stage.erase(it);

        const setEntries setChildren;

        // while (mempoolReferral.exists()) {

        // }


        for (const refiter &childiter : setChildren) {
            if (!setDescendants.count(childiter)) {
                stage.insert(childiter);
            }
        }
    }
}

void ReferralTxMemPool::RemoveRecursive(const Referral &origRef, MemPoolRemovalReason reason)
{
    // Remove referrals from memory pool
    {
        LOCK(cs);
        setEntries toRemove;
        refiter origit = mapRTx.find(origRef.GetHash());
        if (origit != mapRTx.end()) {
            toRemove.insert(origit);
        }

        setEntries setAllRemoves;
        for (refiter it : toRemove) {
            CalculateDescendants(it, setAllRemoves);
        }

        for (const auto& it : setAllRemoves) {
            RemoveStaged(it->GetEntryValue(), reason);
        }
    }
}

/**
 * Called when a block is connected. Removes referrals from mempool.
 */
void ReferralTxMemPool::RemoveForBlock(const std::vector<ReferralRef>& vRefs)
{
    LOCK(cs);

    for (const auto& ref : vRefs) {
        auto it = mapRTx.find(ref->GetHash());

        if(it != mapRTx.end()) {
            NotifyEntryRemoved(it->GetSharedEntryValue(), MemPoolRemovalReason::BLOCK);

            mapRTx.erase(it);
            nReferralsUpdated++;
        }
    }
}

void ReferralTxMemPool::RemoveStaged(const Referral& ref, MemPoolRemovalReason reason)
{
    auto it = mapRTx.find(ref.GetHash());

    if (it != mapRTx.end()) {
        NotifyEntryRemoved(it->GetSharedEntryValue(), reason);

        mapRTx.erase(it);
        nReferralsUpdated++;
    }
}

ReferralRef ReferralTxMemPool::get(const uint256& hash) const
{
    LOCK(cs);
    auto it = mapRTx.find(hash);
    return it != mapRTx.end() ? it->GetSharedEntryValue() : nullptr;
}

ReferralRef ReferralTxMemPool::GetWithCodeHash(const uint256& codeHash) const
{
    LOCK(cs);
    for (const auto& it: mapRTx) {
        const auto ref = it.GetSharedEntryValue();
        if (ref->m_codeHash == codeHash) {
            return ref;
        }
    }

    return nullptr;
}

bool ReferralTxMemPool::ExistsWithCodeHash(const uint256& codeHash) const
{
    if (GetWithCodeHash(codeHash) != nullptr) {
        return true;
    }

    return false;
}

std::vector<ReferralRef> ReferralTxMemPool::GetReferrals() const
{
    LOCK(cs);

    std::vector<ReferralRef> refs(mapRTx.size());


    // for (indexed_transaction_set::const_iterator it = mapTx.begin(); it != mapTx.end(); it++) {
    for (const auto it : mapRTx) {
        refs.push_back(it.GetSharedEntryValue());
    }
    // std::transform(mapRTx.begin(), mapRTx.end(), refs.begin(),
    //         [](indexed_referrals_set::const_iterator it) {
    //             return it->GetSharedEntryValue();
    //         });

    return refs;
}

size_t ReferralTxMemPool::DynamicMemoryUsage() const {
    LOCK(cs);
    return memusage::MallocUsage(sizeof(RefMemPoolEntry) + 15 * sizeof(void*)) * mapRTx.size() + memusage::DynamicUsage(mapLinks);
}

}
