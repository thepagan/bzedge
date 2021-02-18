// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "params.h"

#include <amount.h>
#include <key_io.h>
#include <script/standard.h>
#include "upgrades.h"

namespace Consensus {
    bool Params::NetworkUpgradeActive(int nHeight, Consensus::UpgradeIndex idx) const {
        return NetworkUpgradeState(nHeight, *this, idx) == UPGRADE_ACTIVE;
    }

    bool Params::FutureTimestampSoftForkActive(int nHeight) const {
        return nHeight >= nFutureTimestampSoftForkHeight;
    }

    int Params::Halving(int nHeight) const {
        // zip208
        // Halving(height) :=
        // floor((height - SlowStartShift) / PreBlossomHalvingInterval), if not IsBlossomActivated(height)
        // floor((BlossomActivationHeight - SlowStartShift) / PreBlossomHalvingInterval + (height - BlossomActivationHeight) / PostBlossomHalvingInterval), otherwise
        if (NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BLOSSOM)) {
            int64_t blossomActivationHeight = vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight;
            // Ideally we would say:
            // halvings = (blossomActivationHeight - consensusParams.SubsidySlowStartShift()) / consensusParams.nPreBlossomSubsidyHalvingInterval
            //     + (nHeight - blossomActivationHeight) / consensusParams.nPostBlossomSubsidyHalvingInterval;
            // But, (blossomActivationHeight - consensusParams.SubsidySlowStartShift()) / consensusParams.nPreBlossomSubsidyHalvingInterval
            // would need to be treated as a rational number in order for this to work.
            // Define scaledHalvings := halvings * consensusParams.nPostBlossomSubsidyHalvingInterval;
            int64_t scaledHalvings = ((blossomActivationHeight - SubsidySlowStartShift()) * Consensus::BLOSSOM_POW_TARGET_SPACING_RATIO)
                + (nHeight - blossomActivationHeight);
            return (int) (scaledHalvings / nPostBlossomSubsidyHalvingInterval);
        } else {
            return (nHeight - SubsidySlowStartShift()) / nPreBlossomSubsidyHalvingInterval;
        }
    }

    /**
     * This method determines the block height of the `halvingIndex`th
     * halving, as known at the specified `nHeight` block height.
     *
     * Previous implementations of this logic were specialized to the
     * first halving.
     */
    int Params::HalvingHeight(int nHeight, int halvingIndex) const {
        assert(nHeight >= 0);
        assert(halvingIndex > 0);

        // zip208
        // HalvingHeight(i) := max({ height ⦂ N | Halving(height) < i }) + 1
        //
        // Halving(h) returns the halving index at the specified height.  It is
        // defined as floor(f(h)) where f is a strictly increasing rational
        // function, so it's sufficient to solve for f(height) = halvingIndex
        // in the rationals and then take ceiling(height).
        //
        // H := blossom activation height;
        // SS := SubsidySlowStartShift();
        // R := 1 / (postInterval / preInterval) = BLOSSOM_POW_TARGET_SPACING_RATIO
        // (The following calculation depends on BLOSSOM_POW_TARGET_SPACING_RATIO being an integer.)
        //
        // preBlossom:
        // i = (height - SS) / preInterval
        // height = (preInterval * i) + SS
        //
        // postBlossom:
        // i = (H - SS) / preInterval + (HalvingHeight(i) - H) / postInterval
        // preInterval = postInterval / R
        // i = (H - SS) / (postInterval / R) + (HalvingHeight(i) - H) / postInterval
        // i = (R * (H - SS) + HalvingHeight(i) - H) / postInterval
        // postInterval * i = R * (H - SS) + HalvingHeight(i) - H
        // HalvingHeight(i) = postInterval * i - R * (H - SS) + H
        if (NetworkUpgradeActive(nHeight, Consensus::UPGRADE_BLOSSOM)) {
            int blossomActivationHeight = vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight;

            return
                (nPostBlossomSubsidyHalvingInterval * halvingIndex)
                - (BLOSSOM_POW_TARGET_SPACING_RATIO * (blossomActivationHeight - SubsidySlowStartShift()))
                + blossomActivationHeight;
        } else {
            return (nPreBlossomSubsidyHalvingInterval * halvingIndex) + SubsidySlowStartShift();
        }
    }

    int64_t Params::PoWTargetSpacing(int nHeight) const {        
        bool lwmaActive = nHeight >= nLWMAHeight;
        return lwmaActive ? nPowLwmaTargetSpacing : ZCnPowTargetSpacing;
    }

    int64_t Params::AveragingWindowTimespan(int nHeight) const {
        return nPowAveragingWindow * PoWTargetSpacing(nHeight);
    }

    int64_t Params::MinActualTimespan(int nHeight) const {
        return (AveragingWindowTimespan(nHeight) * (100 - nPowMaxAdjustUp)) / 100;
    }

    int64_t Params::MaxActualTimespan(int nHeight) const {
        return (AveragingWindowTimespan(nHeight) * (100 + nPowMaxAdjustDown)) / 100;
    }
}
