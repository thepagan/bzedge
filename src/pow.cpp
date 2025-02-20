// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "primitives/block.h"
#include "streams.h"
#include "uint256.h"
#include "util.h"

#include <librustzcash.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    // Genesis block / catch undefined block indexes.
    if (pindexLast == NULL)
    {
        return UintToArith256(params.powLimit).GetCompact();
    }
    
    int nHeight = pindexLast->nHeight + 1;

//print logging if the block height is larger than LWMA averaging window.
    if(nHeight > params.nZawyLwmaAveragingWindow) {
		LogPrint("pow", "Zcash Work Required calculation= %d  LWMA calculation = %d  LWMA-3 calculation = %d\n", ZC_GetNextWorkRequired(pindexLast, pblock, params), LwmaGetNextWorkRequired(pindexLast, pblock, params), Lwma3GetNextWorkRequired(pindexLast, pblock, params));
    }

    if (nHeight < params.nLWMAHeight)
    {
		 LogPrint("pow", "DIFF: using Zcash DigiShield\n");
		return ZC_GetNextWorkRequired(pindexLast, pblock, params);
    }
    else if (nHeight < params.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight)
    {
        LogPrint("pow", "DIFF: using LWMA\n");
        return LwmaGetNextWorkRequired(pindexLast, pblock, params);
    }
    else
    {
		LogPrint("pow", "DIFF: using LWMA-3\n");
		return Lwma3GetNextWorkRequired(pindexLast, pblock, params);
	}

}


unsigned int LwmaGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    return LwmaCalculateNextWorkRequired(pindexLast, params);
}


unsigned int LwmaCalculateNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    //Special rule for regtest
    if (params.fPowNoRetargeting) {
        return pindexLast->nBits;
    }

    const int height = pindexLast->nHeight + 1;
    const int64_t T = params.nPowLwmaTargetSpacing; //60
    const int N = params.nZawyLwmaAveragingWindow; //75
    const int k = params.nZawyLwmaAdjustedWeight; //2280
    const int dnorm = params.nZawyLwmaMinDenominator; //10
    const bool limit_st = params.fZawyLwmaSolvetimeLimitation; //true
    assert(height > N);

    arith_uint256 sum_target;
    int t = 0, j = 0;

    // Loop through N most recent blocks.
    for (int i = height - N; i < height; i++) {
        const CBlockIndex* block = pindexLast->GetAncestor(i);
        const CBlockIndex* block_Prev = block->GetAncestor(i - 1);
        int64_t solvetime = block->GetBlockTime() - block_Prev->GetBlockTime();

        if (limit_st && solvetime > 6 * T) {
            solvetime = 6 * T;
        }

        j++;
        t += solvetime * j;  // Weighted solvetime sum.

        // Target sum divided by a factor, (k N^2).
        // The factor is a part of the final equation. However we divide target here to avoid
        // potential overflow.
        arith_uint256 target;
        target.SetCompact(block->nBits);
        sum_target += target / N;
    }

    //Move division to final weighted summed target out of loop to improve precision
    sum_target /= (k * N);

    // Keep t reasonable in case strange solvetimes occurred.
    if (t < N * k / dnorm) {
        t = N * k / dnorm;
    }

    const arith_uint256 pow_limit = UintToArith256(params.powLimit);
    arith_uint256 next_target = t * sum_target;
    if (next_target > pow_limit) {
        next_target = pow_limit;
    }

    return next_target.GetCompact();
}


// LWMA-3 
unsigned int Lwma3GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    return Lwma3CalculateNextWorkRequired(pindexLast, params);
}

unsigned int Lwma3CalculateNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    
    const int64_t T = params.nPreBlossomPowTargetSpacing;
    const int64_t N = params.nZawyLwmaAveragingWindow;
    const int64_t k = N * (N + 1) * T / 2;
    const int64_t height = pindexLast->nHeight;
    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    
    if (height < N) { return powLimit.GetCompact(); }

    arith_uint256 sumTarget, previousDiff, nextTarget;
    int64_t thisTimestamp, previousTimestamp;
    int64_t t = 0, j = 0, solvetimeSum = 0;

    const CBlockIndex* blockPreviousTimestamp = pindexLast->GetAncestor(height - N);
    previousTimestamp = blockPreviousTimestamp->GetBlockTime();

    // Loop through N most recent blocks. 
    for (int64_t i = height - N + 1; i <= height; i++) {
        const CBlockIndex* block = pindexLast->GetAncestor(i);
        thisTimestamp = (block->GetBlockTime() > previousTimestamp) ? block->GetBlockTime() : previousTimestamp + 1;

        int64_t solvetime = std::min(6 * T, thisTimestamp - previousTimestamp);
        previousTimestamp = thisTimestamp;

        j++;
        t += solvetime * j; // Weighted solvetime sum.
        arith_uint256 target;
        target.SetCompact(block->nBits);
        sumTarget += target / (k * N);

        if (i > height - 3) { solvetimeSum += solvetime; } // deprecated
        if (i == height) { previousDiff = target.SetCompact(block->nBits); }
    }

    nextTarget = t * sumTarget;
    
    if (nextTarget > (previousDiff * 150) / 100) { nextTarget = (previousDiff * 150) / 100; }
    if ((previousDiff * 67) / 100 > nextTarget) { nextTarget = (previousDiff * 67)/100; }
    if (solvetimeSum < (8 * T) / 10) { nextTarget = previousDiff * 100 / 106; }
    if (nextTarget > powLimit) { nextTarget = powLimit; }

    return nextTarget.GetCompact();
}

//Orig Zcash function
unsigned int ZC_GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Regtest
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    {
        // Comparing to pindexLast->nHeight with >= because this function
        // returns the work required for the block after pindexLast.
        if (params.nPowAllowMinDifficultyBlocksAfterHeight != std::nullopt &&
            pindexLast->nHeight >= params.nPowAllowMinDifficultyBlocksAfterHeight.value())
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 6 * block interval minutes
            // then allow mining of a min-difficulty block.
            if (pblock && pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.PoWTargetSpacing(pindexLast->nHeight + 1) * 6)
                return nProofOfWorkLimit;
        }
    }

    // Find the first block in the averaging interval
    const CBlockIndex* pindexFirst = pindexLast;
    arith_uint256 bnTot {0};
    for (int i = 0; pindexFirst && i < params.nPowAveragingWindow; i++) {
        arith_uint256 bnTmp;
        bnTmp.SetCompact(pindexFirst->nBits);
        bnTot += bnTmp;
        pindexFirst = pindexFirst->pprev;
    }

    // Check we have enough blocks
    if (pindexFirst == NULL)
        return nProofOfWorkLimit;

    // The protocol specification leaves MeanTarget(height) as a rational, and takes the floor
    // only after dividing by AveragingWindowTimespan in the computation of Threshold(height):
    // <https://zips.z.cash/protocol/protocol.pdf#diffadjustment>
    //
    // Here we take the floor of MeanTarget(height) immediately, but that is equivalent to doing
    // so only after a further division, as proven in <https://math.stackexchange.com/a/147832/185422>.
    arith_uint256 bnAvg {bnTot / params.nPowAveragingWindow};

    return ZC_CalculateNextWorkRequired(bnAvg,
                                     pindexLast->GetMedianTimePast(), pindexFirst->GetMedianTimePast(),
                                     params,
                                     pindexLast->nHeight + 1);
}

unsigned int ZC_CalculateNextWorkRequired(arith_uint256 bnAvg,
                                       int64_t nLastBlockTime, int64_t nFirstBlockTime,
                                       const Consensus::Params& params,
                                       int nextHeight)
{
    int64_t averagingWindowTimespan = params.AveragingWindowTimespan(nextHeight);
    int64_t minActualTimespan = params.MinActualTimespan(nextHeight);
    int64_t maxActualTimespan = params.MaxActualTimespan(nextHeight);
    // Limit adjustment step
    // Use medians to prevent time-warp attacks
    int64_t nActualTimespan = nLastBlockTime - nFirstBlockTime;
    nActualTimespan = averagingWindowTimespan + (nActualTimespan - averagingWindowTimespan)/4;

    if (nActualTimespan < minActualTimespan) {
        nActualTimespan = minActualTimespan;
    }
    if (nActualTimespan > maxActualTimespan) {
        nActualTimespan = maxActualTimespan;
    }

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew {bnAvg};
    bnNew /= averagingWindowTimespan;
    bnNew *= nActualTimespan;

    if (bnNew > bnPowLimit) {
        bnNew = bnPowLimit;
    }

    /// debug print
    LogPrint("pow", "GetNextWorkRequired RETARGET\n");
    LogPrint("pow", "params.AveragingWindowTimespan(%i) = %d    nActualTimespan = %d\n", nextHeight, averagingWindowTimespan, nActualTimespan);
    LogPrint("pow", "Current average: %08x  %s\n", bnAvg.GetCompact(), bnAvg.ToString());
    LogPrint("pow", "After:  %08x  %s\n", bnNew.GetCompact(), bnNew.ToString());

    return bnNew.GetCompact();
}

bool CheckEquihashSolution(const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int n,k;
    size_t nSolSize = pblock->nSolution.size();
    switch (nSolSize){
        case 1344: n=200; k=9; break;
        case 400:  n=192; k=7; break;
        case 100:  n=144; k=5; break;
        case 68:   n=96;  k=5; break;
        case 36:   n=48;  k=5; break;
        default: return error("CheckEquihashSolution: Unsupported solution size of %d", nSolSize);
    }

    LogPrint("pow", "selected n,k : %d, %d \n", n,k);
    eh_HashState state;

    LogPrint("pow", "CURRENT bze_pers_start_blocktime = %d\n", Params().get_bze_pers_start());
    LogPrint("pow", "CURRENT block_time = %d\n", pblock->GetBlockTime());

    if(n == 144 && k == 5)
    {
		if (pblock->GetBlockTime() < Params().get_bze_pers_start())
		{
			EhInitialiseStatePers(n, k, state, "BitcoinZ");
			LogPrint("pow", "PERSONALIZATION STRING: BitcoinZ\n");
		}
		else
		{
			EhInitialiseStatePers(n, k, state, "BZEZhash");
			LogPrint("pow", "PERSONALIZATION STRING: BZEZhash\n");
		}
	}        
    else
    {
		EhInitialiseStatePers(n, k, state, "ZcashPoW");
		LogPrint("pow", "PERSONALIZATION STRING: ZcashPoW\n");
	}  

    // I = the block header minus nonce and solution.
    CEquihashInput I{*pblock};
    // I||V
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    // not going to use librustzcash equihash solution validation, as long as personalization string is hardcoded 
    /*
    return librustzcash_eh_isvalid(
        n, k,
        (unsigned char*)&ss[0], ss.size(),
        pblock->nNonce.begin(), pblock->nNonce.size(),
        pblock->nSolution.data(), pblock->nSolution.size());
    */

    // old way
    ss << pblock->nNonce;

    // H(I||V||...
    state.Update((unsigned char*)&ss[0], ss.size());
    //crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());

    bool isValid;
    EhIsValidSolution(n, k, state, pblock->nSolution, isValid);
    if (!isValid)
    {
        return error("CheckEquihashSolution(): invalid solution");
    }
        

    return true;



}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;
        
    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

arith_uint256 GetBlockProof(const CBlockIndex& block)
{
    arith_uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for an arith_uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (bnTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}

int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params& params)
{
    arith_uint256 r;
    int sign = 1;
    if (to.nChainWork > from.nChainWork) {
        r = to.nChainWork - from.nChainWork;
    } else {
        r = from.nChainWork - to.nChainWork;
        sign = -1;
    }
    r = r * arith_uint256(params.PoWTargetSpacing(tip.nHeight)) / GetBlockProof(tip);
    if (r.bits() > 63) {
        return sign * std::numeric_limits<int64_t>::max();
    }
    return sign * r.GetLow64();
}
