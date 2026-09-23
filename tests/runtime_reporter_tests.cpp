// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <runtime_reporter.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

bool Check(bool condition, const char* name)
{
    if (!condition) {
        std::cerr << "FAIL " << name << '\n';
        return false;
    }

    std::cout << "PASS " << name << '\n';
    return true;
}

} // namespace

int main()
{
    using namespace std::chrono_literals;

    bool ok{true};

    mercaminer::MiningRuntimeStats stats;

    stats.completed_hashes.store(123);
    stats.accepted_blocks.store(2);
    stats.stale_work.store(3);
    stats.rejected_blocks.store(4);
    stats.current_height.store(99);

    const auto snapshot =
        mercaminer::SnapshotMiningRuntimeStats(
            stats);

    ok &= Check(
        snapshot.completed_hashes == 123 &&
            snapshot.accepted_blocks == 2 &&
            snapshot.stale_work == 3 &&
            snapshot.rejected_blocks == 4 &&
            snapshot.current_height == 99,
        "runtime stats snapshot");

    const double rate =
        mercaminer::CalculateHashRate(
            100,
            160,
            2s);

    ok &= Check(
        std::abs(rate - 30.0) < 0.000001,
        "interval hash rate calculation");

    ok &= Check(
        mercaminer::CalculateHashRate(
            10,
            5,
            1s) == 0.0 &&
        mercaminer::CalculateHashRate(
            10,
            20,
            0s) == 0.0,
        "invalid hash-rate intervals fail closed");

    ok &= Check(
        mercaminer::FormatMiningUptime(
            3661s) ==
            "01:01:01",
        "uptime formatting");

    mercaminer::MiningRuntimeStats
        reporter_stats;

    reporter_stats.current_height.store(42);

    std::ostringstream output;

    mercaminer::RuntimeReporter reporter{
        reporter_stats,
        4,
        20ms,
        output};

    reporter_stats.completed_hashes.store(10);
    reporter_stats.accepted_blocks.store(1);
    reporter_stats.stale_work.store(2);
    reporter_stats.rejected_blocks.store(3);

    std::this_thread::sleep_for(
        55ms);

    reporter.Stop();

    const std::string rendered =
        output.str();

    ok &= Check(
        rendered.find("[status]") !=
                std::string::npos &&
            rendered.find("height=42") !=
                std::string::npos &&
            rendered.find("workers=4") !=
                std::string::npos &&
            rendered.find("session_hashes=10") !=
                std::string::npos &&
            rendered.find("accepted=1") !=
                std::string::npos &&
            rendered.find("stale=2") !=
                std::string::npos &&
            rendered.find("rejected=3") !=
                std::string::npos,
        "runtime reporter emits complete periodic status");

    return ok ? 0 : 1;
}
