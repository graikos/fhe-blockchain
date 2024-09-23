#ifndef DIPLO_COINBASE_TRANSACTION_HPP
#define DIPLO_COINBASE_TRANSACTION_HPP

#include "core/transaction.hpp"

#include "sodium.h"

class CoinbaseTransaction : public Transaction
{
public:
    // CoinbaseTransaction(std::vector<std::shared_ptr<Computation>> &comps, const unsigned char *pubkey, uint64_t amount);
    CoinbaseTransaction(const unsigned char *pubkey, uint64_t amount, uint32_t height);

    uint64_t amount();
};

#endif