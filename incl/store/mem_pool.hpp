#ifndef DIPLO_MEM_POOL_HPP
#define DIPLO_MEM_POOL_HPP

#include "store/interface/i_mempool.hpp"
#include "core/transaction.hpp"
#include "core/block.hpp"

#include <thread>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdint>
#include <set>

inline bool comp(const std::vector<unsigned char> &v1, const std::vector<unsigned char> &v2)
{
    return v1 > v2;
}

class MemPool : public IMemPool
{
public:
    MemPool() : tx_set_(comp) {}

    bool add_valid_tx(std::shared_ptr<Transaction> tx) override;
    bool exists(const std::vector<unsigned char> &txid) override;
    bool remove_tx(std::shared_ptr<Transaction> tx) override;
    bool spend_tx(std::shared_ptr<Transaction> tx) override;

    bool add_block(std::shared_ptr<Block> block) override;
    bool spend_block(std::shared_ptr<Block> block) override;

    std::vector<std::shared_ptr<Transaction>> get_top(uint64_t limit) override;
    std::shared_ptr<Transaction> get_tx(const std::vector<unsigned char> &txid) override;

    std::vector<std::string> list_txids() override;

private:
    std::mutex mu_;
    std::set<std::vector<unsigned char>, decltype(comp) *> tx_set_;
    std::unordered_map<std::string, std::shared_ptr<Transaction>> tx_storage_;

    // keeps track of transactions that use specific UTXOs and are in mempool
    std::unordered_map<std::string, std::vector<std::shared_ptr<Transaction>>> utxo_ref_;

    std::string id_to_key(const std::vector<unsigned char> &id);
    std::vector<unsigned char> concat_txid_fee_pair(const std::vector<unsigned char> &txid, uint64_t fee);
    std::vector<unsigned char> txid_from_pair(const std::vector<unsigned char> &concat_pair);

    bool add_valid_tx_unsafe(std::shared_ptr<Transaction> tx);
    bool remove_tx_unsafe(std::shared_ptr<Transaction> tx);
    bool spend_tx_unsafe(std::shared_ptr<Transaction> tx);
};

#endif