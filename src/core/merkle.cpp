#include "core/merkle.hpp"

#include "sodium.h"

std::vector<unsigned char> Merkle::compute_root(std::vector<std::vector<unsigned char>> level_hashes)
{
    if (level_hashes.size() == 1)
    {
        return level_hashes[0];
    }

    if (level_hashes.size() & 1)
    {
        // if odd number in current level, duplicate last
        level_hashes.push_back(level_hashes[level_hashes.size() - 1]);
    }

    std::vector<std::vector<unsigned char>> next_level;
    for (auto it = level_hashes.begin(); it != level_hashes.end(); it += 2)
    {
        std::vector<unsigned char> temp(*it);
        temp.insert(temp.end(), (it + 1)->begin(), (it + 1)->end());
        std::vector<unsigned char> newvec(crypto_generichash_BYTES);
        crypto_generichash(newvec.data(), crypto_generichash_BYTES, temp.data(), temp.size(), nullptr, 0);
        next_level.push_back(newvec);
    }

    return compute_root(std::move(next_level));
}
