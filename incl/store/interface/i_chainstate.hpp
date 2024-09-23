#ifndef DIPLO_I_CHAINSTATE_HPP
#define DIPLO_I_CHAINSTATE_HPP

#include <vector>
#include <cstdint>
#include <memory>
#include <utility>

#include "core/block.hpp"

class IChainstate
{
public:
    virtual bool exists(const std::vector<unsigned char> &txid, uint64_t vout) = 0;

    virtual uint32_t height(const std::vector<unsigned char> &txid, uint64_t vout) = 0;
    virtual bool coinbase(const std::vector<unsigned char> &txid, uint64_t vout) = 0;
    virtual uint64_t amount(const std::vector<unsigned char> &txid, uint64_t vout) = 0;
    virtual std::vector<unsigned char> pubkey(const std::vector<unsigned char> &txid, uint64_t vout) = 0;

    virtual bool add_utxo(const std::vector<unsigned char> &txid, uint64_t vout, uint32_t height, bool coinbase, uint64_t amount, const unsigned char *pubkey) = 0;
    virtual bool remove_utxo(const std::vector<unsigned char> &txid, uint64_t vout, bool save_spent) = 0;

    virtual void add_block(std::shared_ptr<Block> block, uint32_t height) = 0;

    virtual void rewind_block(std::shared_ptr<Block> block) = 0;

    virtual std::vector<std::pair<std::vector<unsigned char>, uint64_t>> filter_by_pubkey(const unsigned char *pubkey) = 0;
};

#endif