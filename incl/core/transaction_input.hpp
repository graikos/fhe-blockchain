#ifndef DIPLO_TRANSACTION_INPUT_HPP
#define DIPLO_TRANSACTION_INPUT_HPP

#include "sodium.h"

#include "message.pb.h"

#include <vector>


class TransactionInput
{
public:
    std::vector<unsigned char> TXID_;
    uint64_t vout_;
    std::vector<unsigned char> sig_;

    TransactionInput(const std::vector<unsigned char> &TXID, uint64_t vout);
    TransactionInput(const std::vector<unsigned char> &TXID, uint64_t vout, uint64_t provided_amount);

    void set_signature(const unsigned char* sig, bool is_temp = false);

    bool is_signed();
    void set_signed(bool new_val);
    void clear_signature();

    uint64_t amount();
    void set_amount(uint64_t am);

    void set_temp_sig_size(uint32_t temp_size);
    void revert_sig_size();
    
    std::vector<unsigned char> serialize();

    ProtoTransactionInput to_proto() const;

    static TransactionInput from_proto(const ProtoTransactionInput &pi, bool is_for_coinbase = false);

private:
    bool is_signed_;
    uint64_t amount_ = 0;
    uint32_t sig_size_;
};

#endif