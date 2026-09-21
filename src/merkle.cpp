// Copyright (c) 2026 The MercaMiner developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <merkle.h>

#include <hash256.h>

#include <array>
#include <cstddef>
#include <span>
#include <utility>

namespace mercaminer {

MerkleResult ComputeMerkleRoot(
    std::vector<UInt256> hashes)
{
    bool mutated{false};

    while (hashes.size() > 1) {
        for (std::size_t i = 0;
             i + 1 < hashes.size();
             i += 2) {
            if (hashes[i] == hashes[i + 1]) {
                mutated = true;
            }
        }

        if (hashes.size() % 2 != 0) {
            hashes.push_back(hashes.back());
        }

        std::vector<UInt256> next;
        next.reserve(hashes.size() / 2);

        for (std::size_t i = 0;
             i < hashes.size();
             i += 2) {
            std::array<unsigned char, 64> pair{};

            for (std::size_t j = 0; j < 32; ++j) {
                pair[j] =
                    hashes[i].bytes()[j];
                pair[32 + j] =
                    hashes[i + 1].bytes()[j];
            }

            next.push_back(
                DoubleSha256(
                    std::span<const unsigned char>{
                        pair}));
        }

        hashes = std::move(next);
    }

    if (hashes.empty()) {
        return MerkleResult{
            UInt256{},
            mutated};
    }

    return MerkleResult{
        hashes.front(),
        mutated};
}

} // namespace mercaminer
