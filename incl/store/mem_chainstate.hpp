#ifndef DIPLO_MEM_CHAINSTATE_HPP
#define DIPLO_MEM_CHAINSTATE_HPP

#include "store/interface/i_chainstate.hpp"

#include "sodium.h"

#include <thread>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

class UTXORecord
{
public:
    uint32_t height_;
    bool coinbase_;
    uint64_t amount_;
    std::vector<unsigned char> pubkey_;

    UTXORecord(uint32_t height, bool coinbase, uint64_t amount, const unsigned char *pubkey)
        : height_(height), coinbase_(coinbase), amount_(amount), pubkey_(pubkey, pubkey + crypto_sign_PUBLICKEYBYTES)
    {
    }
};

class SpentSet;

class MemChainstate : public IChainstate
{
public:
    MemChainstate() : spent_set_(std::make_unique<SpentSet>()) {}

    bool exists(const std::vector<unsigned char> &txid, uint64_t vout) override;

    uint32_t height(const std::vector<unsigned char> &txid, uint64_t vout) override;
    bool coinbase(const std::vector<unsigned char> &txid, uint64_t vout) override;
    uint64_t amount(const std::vector<unsigned char> &txid, uint64_t vout) override;
    std::vector<unsigned char> pubkey(const std::vector<unsigned char> &txid, uint64_t vout) override;

    bool add_utxo(const std::vector<unsigned char> &txid, uint64_t vout, uint32_t height, bool is_coinbase, uint64_t amount, const unsigned char *pubkey) override;
    bool remove_utxo(const std::vector<unsigned char> &txid, uint64_t vout, bool save_spent = true) override;

    void add_block(std::shared_ptr<Block> block, uint32_t height) override;

    void rewind_block(std::shared_ptr<Block> block) override;

    std::vector<std::pair<std::vector<unsigned char>, uint64_t>> filter_by_pubkey(const unsigned char *pubkey) override;

private:
    std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<UTXORecord>> storage_;

    std::string pair_to_key(const std::vector<unsigned char> &txid, uint64_t vout);

    std::unique_ptr<SpentSet> spent_set_;
};

class SpentSet
{
public:
    std::unordered_map<std::string, std::shared_ptr<UTXORecord>> spent_storage_;
    bool add_spent_utxo(const std::vector<unsigned char> &txid, uint64_t vout, std::shared_ptr<UTXORecord> record);
    std::shared_ptr<UTXORecord> get_spent_utxo(const std::vector<unsigned char> &txid, uint64_t vout);
    bool remove_spent_utxo(const std::vector<unsigned char> &txid, uint64_t vout);
    SpentSet() = default;
};

#endif