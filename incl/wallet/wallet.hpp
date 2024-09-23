#ifndef DIPLO_WALLET_HPP
#define DIPLO_WALLET_HPP

#include "nlohmann/json.hpp"
#include "sodium.h"
#include "base64.hpp"
#include "util/util.hpp"

#include "core/transaction_output.hpp"
#include "core/transaction_input.hpp"
#include "core/block.hpp"
#include "core/transaction.hpp"

#include "store/interface/i_blockstore.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <thread>

using json = nlohmann::json;

class Wallet
{
public:
    std::vector<unsigned char> public_key_;
    std::vector<unsigned char> secret_key_;

    Wallet();
    Wallet(const json &config);

    std::shared_ptr<Transaction> new_transaction(const std::vector<unsigned char> &rec_pub_key, uint64_t amount, uint64_t fee = 0);

    void filter_transaction(std::shared_ptr<Transaction> tx);
    void spend_transaction(std::shared_ptr<Transaction> tx);
    void filter_block(std::shared_ptr<Block> block);
    void spend_block(std::shared_ptr<Block> block);

    void rescan(const std::vector<std::shared_ptr<BlockHeader>> &chain, std::shared_ptr<IBlockStore> block_store);

private:
    std::mutex wallet_mu_;
    std::unordered_map<std::string, std::shared_ptr<TransactionInput>> coins_;

    void gen_keys();
    void unsafe_filter_transaction(std::shared_ptr<Transaction> tx);
    void unsafe_spend_transaction(std::shared_ptr<Transaction> tx);
    bool unsafe_remove_coin(const std::vector<unsigned char> &txid, uint64_t vout);
};

#endif
