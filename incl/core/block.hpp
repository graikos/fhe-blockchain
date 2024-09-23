#ifndef DIPLO_BLOCK_HPP
#define DIPLO_BLOCK_HPP

#include "core/block_header.hpp"
#include "core/transaction.hpp"

#include <vector>

#include "message.pb.h"

class Block
{
public:
    std::shared_ptr<BlockHeader> header_;
    std::vector<std::shared_ptr<Transaction>> transactions_;

    Block() = default;
    Block(std::shared_ptr<BlockHeader> prev_block_header, uint32_t difficulty, const std::vector<std::shared_ptr<Computation>> &computations,
          const std::vector<std::shared_ptr<Transaction>> &transactions);

    // genesis
    Block(std::time_t timestamp, uint32_t difficulty, const unsigned char *pubkey, uint64_t amount);

    std::vector<unsigned char> serialize();
    std::vector<unsigned char> hash(bool force = false);

    ProtoBlock to_proto() const;

    static Block from_proto(const ProtoBlock &proto, const ComputationFactory& comp_factory);
};

#endif