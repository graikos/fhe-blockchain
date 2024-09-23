#ifndef DIPLO_MERKLE_HPP
#define DIPLO_MERKLE_HPP

#include <vector>

class Merkle
{
public:
    Merkle() = default;

    static std::vector<unsigned char> compute_root(std::vector<std::vector<unsigned char>> level_hashes);
};

#endif