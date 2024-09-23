#ifndef DIPLO_TRANSACTION_HPP
#define DIPLO_TRANSACTION_HPP

#include <vector>
#include <memory>

#include "sodium.h"

#include "message.pb.h"

#include "core/transaction_input.hpp"
#include "core/transaction_output.hpp"

class Transaction
{
public:
    std::vector<std::shared_ptr<TransactionInput>> inputs_;
    std::vector<std::shared_ptr<TransactionOutput>> outputs_;

    Transaction() = default;
    Transaction(const std::vector<std::shared_ptr<TransactionInput>> &inputs, const std::vector<std::shared_ptr<TransactionOutput>> &outputs);
    Transaction(const std::vector<std::shared_ptr<TransactionInput>> &inputs, const unsigned char *self_pubkey, const unsigned char *rec_pubkey, uint64_t amount, uint64_t fee);

    Transaction(const Transaction &other);

    // Overload the equality operator, without const because TXID may need to be computed
    bool operator==(Transaction &other);

    std::vector<unsigned char> TXID();

    void add_input(std::shared_ptr<TransactionInput> new_input);
    void add_output(std::shared_ptr<TransactionOutput> new_output);

    std::vector<unsigned char> serialize();

    void sign(const unsigned char *self_pubkey, const unsigned char *privkey);

    bool validate_transaction(const std::vector<std::vector<unsigned char>> &pubkeys) const;
    // void sign_transaction(const std::vector<unsigned char> &pubkey, const unsigned char *secret_key);

    uint64_t fee();

    virtual ~Transaction() = default;

    ProtoTransaction to_proto() const;
    static Transaction from_proto(const ProtoTransaction &proto, bool is_coinbase);

protected:
    bool has_txid_;
    std::vector<unsigned char> TXID_;

    void compute_txid();
};

#endif