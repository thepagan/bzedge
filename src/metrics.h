// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_METRICS_H
#define ZCASH_METRICS_H

#include "uint256.h"
#include "consensus/params.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>

struct AtomicCounter {
    std::atomic<uint64_t> value;

    AtomicCounter() : value {0} { }

    void increment(){
        ++value;
    }

    void decrement(){
        --value;
    }

    int get() const {
        return value.load();
    }
};

class AtomicTimer {
private:
    std::mutex mtx;
    uint64_t threads;
    int64_t start_time;
    int64_t total_time;

public:
    AtomicTimer() : threads(0), start_time(0), total_time(0) {}

    /**
     * Starts timing on first call, and counts the number of calls.
     */
    void start();

    /**
     * Counts number of calls, and stops timing after it has been called as
     * many times as start().
     */
    void stop();

    bool running();

    uint64_t threadCount();

    double rate(const AtomicCounter& count);
};

enum DurationFormat {
    FULL,
    REDUCED
};

extern AtomicCounter transactionsValidated;
extern AtomicCounter ehSolverRuns;
extern AtomicCounter solutionTargetChecks;
extern AtomicTimer miningTimer;
extern std::atomic<size_t> nSizeReindexed; // valid only during reindex
extern std::atomic<size_t> nFullSizeToReindex; // valid only during reindex

void TrackMinedBlock(uint256 hash);

void MarkStartTime();
double GetLocalSolPS();
int EstimateNetHeight(const Consensus::Params& params, int currentBlockHeight, int64_t currentBlockTime);
int MyEstimateNetHeight();
void MarkDownloadStart(int height);
int64_t GetDownloadSpeed(int height);
std::optional<int64_t> SecondsLeftToNextEpoch(const Consensus::Params& params, int currentHeight);
std::string DisplayDuration(int64_t time, DurationFormat format);
std::string DisplaySize(size_t value);
std::string DisplayHashRate(double value);

void TriggerRefresh();

void ConnectMetricsScreen();
void ThreadShowMetricsScreen();

const std::string METRICS_ART =
"\e[1;34m       _/_/_/    _/_/_/_/_/  _/_/_/_/        _/                     \n"
"      _/    _/        _/    _/          _/_/_/    _/_/_/    _/_/    \n"
"     _/_/_/        _/      _/_/_/    _/    _/  _/    _/  _/_/_/_/   \n"
"    _/    _/    _/        _/        _/    _/  _/    _/  _/          \n"
"   _/_/_/    _/_/_/_/_/  _/_/_/_/    _/_/_/    _/_/_/    _/_/_/     \n"
"                                                  _/                \n"
"                                             _/_/                   \e[0m\n";

#endif // ZCASH_METRICS_H
