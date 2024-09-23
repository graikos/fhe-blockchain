#ifndef DIPLO_FHE_COMPUTATION_HPP
#define DIPLO_FHE_COMPUTATION_HPP

#include <string>
#include <vector>
#include <ctime>

#include "nlohmann/json.hpp"
#include "openfhe.h"
#include "scheme/bgvrns/bgvrns-ser.h"
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"

#include "message.pb.h"

using json = nlohmann::json;
using namespace lbcrypto;

class FHEComputation
{
public:
    std::string expression_;
    PublicKey<DCRTPoly> publicKey_;
    // Ciphertext<DCRTPoly> is already a shared pointer
    std::vector<Ciphertext<DCRTPoly>> ciphertexts_;
    std::time_t timestamp_;

    FHEComputation() = default;
    FHEComputation(const json &computation_json);

    CryptoContext<DCRTPoly> GetCryptoContext();

    /**
     * @brief This method will add encryptions of zero to the input ciphertexts.
     *
     * Using the data along with a counter as a seed, encryptions of zero will be
     * generated determinstically and added homomorphically to the ciphertexts.
     */
    void bind_inputs_to_data(const std::vector<unsigned char> &data);

    std::vector<unsigned char> serialize();

    std::vector<Ciphertext<DCRTPoly>> unbound_ciphertexts_archive_;
    bool is_bound_;

    static FHEComputation from_proto(const ProtoComputation &proto);
};

#endif