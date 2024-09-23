#include "core/block_header.hpp"

#include "util/util.hpp"

#include "sodium.h"

#include "base64.hpp"

BlockHeader::BlockHeader(std::shared_ptr<BlockHeader> prev_block_header, const std::vector<unsigned char> &merkle_root, std::time_t timestamp, uint32_t difficulty, const std::vector<std::shared_ptr<Computation>> &computations)
    : prev_block_header_(prev_block_header), merkle_root_(merkle_root), timestamp_(timestamp), difficulty_(difficulty), computations_(computations)
{
}

BlockHeader::BlockHeader(const std::vector<unsigned char> &prev_hash, const std::vector<unsigned char> &merkle_root, std::time_t timestamp, uint32_t difficulty, const std::vector<std::shared_ptr<Computation>> &computations)
    : prev_block_header_(nullptr), merkle_root_(merkle_root), timestamp_(timestamp), difficulty_(difficulty), computations_(computations), prev_hash_(prev_hash)
{
}

std::vector<unsigned char> BlockHeader::serialize(bool include_proofs)
{
    // - Previous block hash
    // - Merkle root (should also contain the "coinbase" transactions)
    // - Timestamp
    // - Difficulty (in my case, minimum depth)
    // - Computation count (8 bytes)
    // - For each computation
    // 	- Computation size
    // 	- Serialized computation
    // - For each computation
    // 	- Proof size
    // 	- Proof

    std::vector<unsigned char> res = prev_hash();

    res.insert(res.end(), merkle_root_.begin(), merkle_root_.end());
    auto t = util::uint64_to_vector_big_endian(timestamp_);
    res.insert(res.end(), t.begin(), t.end());
    auto d = util::uint32_to_vector_big_endian(difficulty_);
    res.insert(res.end(), d.begin(), d.end());
    auto count = util::uint64_to_vector_big_endian(computations_.size());
    res.insert(res.end(), count.begin(), count.end());
    for (const auto &comp : computations_)
    {
        // include_proofs being false means we are serializing to get binding data
        // which means we also don't want the serialized computation to include the output
        auto cser = comp->serialize(include_proofs);
        auto csize = util::uint64_to_vector_big_endian(cser.size());

        res.insert(res.end(), csize.begin(), csize.end());
        res.insert(res.end(), cser.begin(), cser.end());
    }

    // this allows serialization to produce binding data, since we don't want to include
    // proofs in that case
    if (include_proofs)
    {
        for (const auto &comp : computations_)
        {
            auto pser = comp->proof();
            auto psize = util::uint64_to_vector_big_endian(pser.size());
            res.insert(res.end(), psize.begin(), psize.end());
            res.insert(res.end(), pser.begin(), pser.end());
        }
    }
    return res;
}

std::vector<unsigned char> BlockHeader::hash(bool force)
{
    if (has_hash_ && !force)
    {
        return hash_;
    }

    auto ser = serialize();

    hash_.resize(crypto_generichash_BYTES);
    crypto_generichash(hash_.data(), crypto_generichash_BYTES, ser.data(), ser.size(), nullptr, 0);
    has_hash_ = true;

    return hash_;
}

std::vector<unsigned char> BlockHeader::prev_hash()
{
    if (prev_block_header_)
    {
        // block has concrete pointer to previous header
        return prev_block_header_->hash();
    }

    if (prev_hash_.size() > 0)
    {
        // block only knows previous header hash
        return prev_hash_;
    }

    // block does not have previous, use 0 instead (coinbase)
    return std::vector<unsigned char>(crypto_generichash_BYTES, 0);
}

ProtoBlockHeader BlockHeader::to_proto() const
{
    ProtoBlockHeader pbh;
    // TODO: maybe this should call prev_hash()
    auto prevh = prev_block_header_->hash();
    pbh.set_prev_block_hash(std::string(prevh.begin(), prevh.end()));

    pbh.set_merkle_root(std::string(merkle_root_.begin(), merkle_root_.end()));

    pbh.set_timestamp(timestamp_);

    pbh.set_difficulty(difficulty_);

    for (const auto &comp : computations_)
    {
        pbh.add_computations()->CopyFrom(comp->to_proto());
    }

    return pbh;
}

/*
message ProtoBlockHeader {
    bytes prev_block_hash = 1;
    bytes merkle_root = 2;
    int64 timestamp = 3;
    uint32 difficulty = 4;
    repeated ProtoComputation computations = 5;
}
*/

BlockHeader BlockHeader::from_proto(const ProtoBlockHeader &proto, const ComputationFactory &comp_factory)
{
    // BlockHeader(const std::vector<unsigned char> &prev_hash, const std::vector<unsigned char> &merkle_root,
    // std::time_t timestamp, uint32_t difficulty, const std::vector<std::shared_ptr<Computation>> &computations);

    BlockHeader bh;

    bh.prev_hash_ = std::vector<unsigned char>(proto.prev_block_hash().begin(), proto.prev_block_hash().end());
    bh.merkle_root_ = std::vector<unsigned char>(proto.merkle_root().begin(), proto.merkle_root().end());
    bh.timestamp_ = proto.timestamp();
    bh.difficulty_ = proto.difficulty();

    for (const auto &proto_comp : proto.computations())
    {
        bh.computations_.push_back(comp_factory.createComputation(proto_comp));
    }

    std::cout << "Inside header from_proto, prev_hash: " << base64::encode(bh.prev_hash_.data(), bh.prev_hash_.size()) << std::endl;

    return bh;
}