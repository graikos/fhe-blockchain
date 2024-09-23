#ifndef DIPLO_BLOCK_HEADER_HPP
#define DIPLO_BLOCK_HEADER_HPP

#include <vector>
#include <memory>
#include <ctime>

#include "core/interface/computation.hpp"

#include "message.pb.h"

class BlockHeader
{
public:
    std::shared_ptr<BlockHeader> prev_block_header_;
    std::vector<unsigned char> merkle_root_;
    std::time_t timestamp_;
    uint32_t difficulty_;
    std::vector<std::shared_ptr<Computation>> computations_;

    BlockHeader() = default;
    BlockHeader(std::shared_ptr<BlockHeader> prev_block_header, const std::vector<unsigned char> &merkle_root, std::time_t timestamp, uint32_t difficulty, const std::vector<std::shared_ptr<Computation>> &computations);
    BlockHeader(const std::vector<unsigned char> &prev_hash, const std::vector<unsigned char> &merkle_root, std::time_t timestamp, uint32_t difficulty, const std::vector<std::shared_ptr<Computation>> &computations);

    std::vector<unsigned char> serialize(bool include_proofs = true);
    std::vector<unsigned char> hash(bool force = false);

    std::vector<unsigned char> prev_hash();

    ProtoBlockHeader to_proto() const;

    static BlockHeader from_proto(const ProtoBlockHeader &proto, const ComputationFactory &comp_factory);

private:
    bool has_hash_ = false;
    std::vector<unsigned char> hash_;
    std::vector<unsigned char> prev_hash_;
};

#endif