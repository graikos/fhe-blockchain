#include <iostream>
#include "core/block.hpp"

#include "core/merkle.hpp"
#include "util/util.hpp"
#include "core/coinbase_transaction.hpp"

#include <ctime>

#include "base64.hpp"

Block::Block(std::shared_ptr<BlockHeader> prev_block_header, uint32_t difficulty,
             const std::vector<std::shared_ptr<Computation>> &computations,
             const std::vector<std::shared_ptr<Transaction>> &transactions) : transactions_(transactions)
{
    // NOTE: we assume that in this vector a coinbase transaction is included
    std::vector<std::vector<unsigned char>> tx_vec;
    for (const auto &tx : transactions_)
    {
        tx_vec.push_back(tx->TXID());
    }
    auto merkle_root = Merkle::compute_root(std::move(tx_vec));

    header_ = std::make_shared<BlockHeader>(prev_block_header, merkle_root, std::time(0), difficulty, computations);
}

std::vector<unsigned char> Block::serialize()
{
    // - Serialized header
    // - Transaction count (8 bytes)
    // - Transactions

    std::vector<unsigned char> res = header_->serialize();

    auto txcount = util::uint64_to_vector_big_endian(transactions_.size());
    res.insert(res.end(), txcount.begin(), txcount.end());

    for (const auto &tx : transactions_)
    {
        auto txser = tx->serialize();
        res.insert(res.end(), txser.begin(), txser.end());
    }
    return res;
}

std::vector<unsigned char> Block::hash(bool force)
{
    // hash of the block is just the hash of the header, since all the info is combined there
    return header_->hash(force);
}

Block::Block(std::time_t timestamp, uint32_t difficulty, const unsigned char *pubkey, uint64_t amount)
{
    // genesis has height 0
    auto cb = std::make_shared<CoinbaseTransaction>(pubkey, amount, 0);
    transactions_.push_back(cb);
    std::vector<std::vector<unsigned char>> tx_vec;
    for (const auto &tx : transactions_)
    {
        tx_vec.push_back(tx->TXID());
    }
    auto merkle_root = Merkle::compute_root(std::move(tx_vec));
    const std::vector<std::shared_ptr<Computation>> comps;
    std::shared_ptr<BlockHeader> prev(nullptr);
    header_ = std::make_shared<BlockHeader>(prev, merkle_root, timestamp, difficulty, comps);
}

ProtoBlock Block::to_proto() const
{
    ProtoBlock pb;
    pb.mutable_header()->CopyFrom(header_->to_proto());

    for (const auto &tx : transactions_)
    {
        pb.add_transactions()->CopyFrom(tx->to_proto());
    }
    return pb;
}

/*
message ProtoBlock {
    ProtoBlockHeader header = 1;
    repeated ProtoTransaction transactions = 2;
}
*/

Block Block::from_proto(const ProtoBlock &proto, const ComputationFactory &comp_factory)
{
    Block b;
    b.header_ = std::make_shared<BlockHeader>(BlockHeader::from_proto(proto.header(), comp_factory));

    // first transaction is cb
    if (!proto.transactions().empty())
    {
        b.transactions_.push_back(std::make_shared<Transaction>(Transaction::from_proto(proto.transactions(0), true)));
    }

    for (int i = 1; i < proto.transactions_size(); ++i)
    {
        b.transactions_.push_back(std::make_shared<Transaction>(Transaction::from_proto(proto.transactions(i), false)));
    }
    return b;
}