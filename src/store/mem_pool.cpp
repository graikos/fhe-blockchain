#include "store/mem_pool.hpp"

#include <cassert>

#include "util/util.hpp"

bool MemPool::add_valid_tx(std::shared_ptr<Transaction> tx)
{
    std::lock_guard<std::mutex> lg(mu_);
    return add_valid_tx_unsafe(tx);
}

bool MemPool::add_valid_tx_unsafe(std::shared_ptr<Transaction> tx)
{
    auto txid = tx->TXID();
    auto key = id_to_key(txid);

    if (tx_storage_.find(key) != tx_storage_.end())
    {
        // transaction already exists in mem pool
        return false;
    }

    // add to tx key/value storage
    tx_storage_[key] = tx;

    // add to set with fee
    tx_set_.insert(concat_txid_fee_pair(txid, tx->fee()));

    // add UTXO to ref map
    for (auto &outp : tx->outputs_)
    {
        auto utxo_key = id_to_key(outp->hash());
        if (utxo_ref_.find(utxo_key) == utxo_ref_.end())
        {
            // entry for this utxo does not exist
            utxo_ref_[utxo_key] = std::vector<std::shared_ptr<Transaction>>{tx};
        }
        else
        {
            utxo_ref_[utxo_key].push_back(tx);
        }
    }
    return true;
}

bool MemPool::remove_tx(std::shared_ptr<Transaction> tx)
{
    std::lock_guard<std::mutex> lg(mu_);
    return remove_tx_unsafe(tx);
}

bool MemPool::remove_tx_unsafe(std::shared_ptr<Transaction> tx)
{
    auto txid = tx->TXID();
    auto key = id_to_key(txid);

    if (tx_storage_.find(key) == tx_storage_.end())
    {
        // transaction does not exists in mem pool
        return false;
    }

    tx_storage_.erase(key);
    tx_set_.erase(concat_txid_fee_pair(txid, tx->fee()));
    for (auto &outp : tx->outputs_)
    {
        auto utxo_key = id_to_key(outp->hash());
        // if utxo cannot be found in ref, logic is wrong somewhere
        assert(utxo_ref_.find(utxo_key) != utxo_ref_.end());

        if (utxo_ref_[utxo_key].size() == 1)
        {
            utxo_ref_.erase(utxo_key);
        }
        else
        {
            auto &record = utxo_ref_[utxo_key];
            auto it = record.begin();
            bool found = false;
            for (; it != record.end(); ++it)
            {
                if ((**it) == *tx)
                {
                    found = true;
                    break;
                }
            }
            assert(found);
            record.erase(it);
        }
    }

    return true;
}

bool MemPool::spend_tx(std::shared_ptr<Transaction> tx)
{
    std::lock_guard<std::mutex> lg(mu_);
    return spend_tx_unsafe(tx);
}

bool MemPool::spend_tx_unsafe(std::shared_ptr<Transaction> tx)
{
    auto txid = tx->TXID();
    auto key = id_to_key(txid);

    if (tx_storage_.find(key) == tx_storage_.end())
    {
        // transaction does not exists in mem pool
        return false;
    }

    tx_storage_.erase(key);
    tx_set_.erase(concat_txid_fee_pair(txid, tx->fee()));
    for (auto &outp : tx->outputs_)
    {
        auto utxo_key = id_to_key(outp->hash());
        // if utxo cannot be found in ref, logic is wrong somewhere
        assert(utxo_ref_.find(utxo_key) != utxo_ref_.end());

        // find every other TX in mempool that references this UTXO
        for (const auto &othertx : utxo_ref_[utxo_key])
        {
            if (othertx == tx)
            {
                continue;
            }

            auto otherkey = id_to_key(othertx->TXID());
            tx_storage_.erase(otherkey);
            tx_set_.erase(concat_txid_fee_pair(othertx->TXID(), othertx->fee()));
        }
        utxo_ref_.erase(utxo_key);
    }

    return true;
}

bool MemPool::spend_block(std::shared_ptr<Block> block)
{
    std::lock_guard<std::mutex> lg(mu_);
    bool is_cb = true;
    for (const auto &tx : block->transactions_)
    {
        if (is_cb)
        {
            // skip coinbase of block
            is_cb = false;
            continue;
        }
        // here we don't care if the TX exists in the mem pool or not
        // since a block may have TX that are not in our mem pool
        spend_tx_unsafe(tx);
    }
    return true;
}

bool MemPool::add_block(std::shared_ptr<Block> block)
{
    std::lock_guard<std::mutex> lg(mu_);
    bool is_cb = true;
    for (const auto &tx : block->transactions_)
    {
        if (is_cb)
        {
            // skip coinbase of block
            is_cb = false;
            continue;
        }
        if (!(add_valid_tx_unsafe(tx)))
        {
            return false;
        }
    }
    return true;
}

std::vector<std::shared_ptr<Transaction>> MemPool::get_top(uint64_t limit)
{
    std::lock_guard<std::mutex> lg(mu_);
    std::vector<std::shared_ptr<Transaction>> res;
    auto it = tx_set_.begin();
    // NOTE: for 100% correct, here no TX that reference same UTXO should be returned, just like
    // in validate_block
    for (uint64_t i = 0; i < limit && it != tx_set_.end(); ++i, ++it)
    {
        res.push_back(tx_storage_.at(id_to_key(txid_from_pair(*it))));
    }
    return res;
}

std::string MemPool::id_to_key(const std::vector<unsigned char> &id)
{
    return std::string(id.begin(), id.end());
}

std::vector<unsigned char> MemPool::concat_txid_fee_pair(const std::vector<unsigned char> &txid, uint64_t fee)
{
    // to take advantage of lex. compare of vectors, and since bytes are big endian from util
    // we will concat serialized fee + txid. The comparison will sort based on fee first,
    // but even if they have equal fees, having different TXID will allow to insert
    std::vector<unsigned char> res(txid.size() + 8);
    auto feeser = util::uint64_to_vector_big_endian(fee);
    std::copy(feeser.begin(), feeser.end(), res.begin());
    std::copy(txid.begin(), txid.end(), res.begin() + 8);
    return res;
}

std::vector<unsigned char> MemPool::txid_from_pair(const std::vector<unsigned char> &concat_pair)
{
    return std::vector<unsigned char>(concat_pair.begin() + 8, concat_pair.end());
}

std::shared_ptr<Transaction> MemPool::get_tx(const std::vector<unsigned char> &txid)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = id_to_key(txid);
    return tx_storage_.at(key);
}

bool MemPool::exists(const std::vector<unsigned char> &txid)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = id_to_key(txid);
    return tx_storage_.find(key) != tx_storage_.end();
}

std::vector<std::string> MemPool::list_txids()
{
    std::lock_guard<std::mutex> lg(mu_);
    std::vector<std::string> res;
    for (const auto &p : tx_storage_)
    {
        res.push_back(p.first);
    }
    return res;
}