#include "core/transaction_input.hpp"
#include <iostream>

#include "util/util.hpp"

TransactionInput::TransactionInput(const std::vector<unsigned char> &TXID, uint64_t vout) : TXID_(TXID), vout_(vout), is_signed_(false), sig_size_(crypto_sign_BYTES)
{
}

TransactionInput::TransactionInput(const std::vector<unsigned char> &TXID, uint64_t vout, uint64_t provided_amount) : TXID_(TXID), vout_(vout), is_signed_(false), amount_(provided_amount), sig_size_(crypto_sign_BYTES)
{
}

void TransactionInput::set_signature(const unsigned char *sig, bool is_temp)
{
    sig_.resize(sig_size_);
    std::copy(sig, sig + sig_size_, sig_.begin());
    if (!is_temp)
    {
        // do not set as signed if we just replaced the sig field with the pubkey during signing of tx
        is_signed_ = true;
    }
}

bool TransactionInput::is_signed()
{
    return is_signed_;
}

void TransactionInput::set_signed(bool newval)
{
    is_signed_ = newval;
}

uint64_t TransactionInput::amount()
{
    return amount_;
}

void TransactionInput::set_temp_sig_size(uint32_t temp_size)
{
    sig_size_ = temp_size;
}

void TransactionInput::revert_sig_size()
{
    sig_size_ = crypto_sign_BYTES;
}

std::vector<unsigned char> TransactionInput::serialize()
{

    // 32 (TXID) + 8 (vout) + signature size (or temp element size at sig field) + sig
    std::vector<unsigned char> res(32 + 8 + 4 + sig_size_);

    std::copy(TXID_.begin(), TXID_.end(), res.begin());

    util::uint64_to_uchar_big_endian(vout_, res.data() + 32);
    util::uint32_to_uchar_big_endian(sig_size_, res.data() + 40);

    // if no signature, replace with 0
    if (sig_.size() == 0)
    {
        sig_ = std::vector<unsigned char>(sig_size_, 0);
    }
    std::copy(sig_.begin(), sig_.end(), res.begin() + 44);

    return res;
}

void TransactionInput::clear_signature()
{
    sig_ = std::vector<unsigned char>();
    is_signed_ = false;
}

void TransactionInput::set_amount(uint64_t am)
{
    amount_ = am;
}

ProtoTransactionInput TransactionInput::to_proto() const
{
    ProtoTransactionInput pi;
    pi.set_txid(std::string(TXID_.begin(), TXID_.end()));
    pi.set_vout(vout_);
    pi.set_sig(std::string(sig_.begin(), sig_.end()));
    return pi;
}

TransactionInput TransactionInput::from_proto(const ProtoTransactionInput &pi, bool is_for_coinbase)
{
    std::vector<unsigned char> txid(pi.txid().begin(), pi.txid().end());

    TransactionInput res(txid, pi.vout());

    if (!pi.sig().empty())
    {
        if (is_for_coinbase)
        {
            res.set_temp_sig_size(4);
        }
        res.set_signature(reinterpret_cast<const unsigned char *>(pi.sig().c_str()));
    }
    else
    {
        std::cout << "input sig from proto is empty" << std::endl;
    }

    return res;
}