#ifndef DIPLO_I_MEMPOOL_HPP
#define DIPLO_I_MEMPOOL_HPP

#include "core/transaction.hpp"
#include "core/block.hpp"

class IMemPool
{
public:
    virtual bool add_valid_tx(std::shared_ptr<Transaction> tx) = 0;
    virtual bool remove_tx(std::shared_ptr<Transaction> tx) = 0;
    virtual bool exists(const std::vector<unsigned char> &txid) = 0;
    virtual bool add_block(std::shared_ptr<Block> block) = 0;
    virtual bool spend_block(std::shared_ptr<Block> block) = 0;
    virtual bool spend_tx(std::shared_ptr<Transaction> tx) = 0;
    virtual std::vector<std::shared_ptr<Transaction>> get_top(uint64_t limit) = 0;
    virtual std::shared_ptr<Transaction> get_tx(const std::vector<unsigned char> &txid) = 0;
    virtual std::vector<std::string> list_txids() = 0;
};

#endif