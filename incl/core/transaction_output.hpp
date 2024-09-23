#ifndef DIPLO_TRANSACTION_OUTPUT_HPP
#define DIPLO_TRANSACTION_OUTPUT_HPP

#include "sodium.h"

#include "message.pb.h"

#include <vector>

class TransactionOutput
{
public:
    uint64_t amount_;
    std::vector<unsigned char> public_key_;

    TransactionOutput(uint64_t amount, const unsigned char *pubkey);

    std::vector<unsigned char> serialize();

    std::vector<unsigned char> hash(bool force=false);

    ProtoTransactionOutput to_proto() const;

    static TransactionOutput from_proto(const ProtoTransactionOutput& proto);

private:
    bool has_hash_ = false;
    std::vector<unsigned char> hash_;
};

#endif