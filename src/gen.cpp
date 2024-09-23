#include <openfhe.h>
#include <ctime>
#include <iostream>
#include <sstream>
#include <ostream>
#include <istream>
#include "base64.hpp"
#include "nlohmann/json.hpp"
#include <scheme/bgvrns/bgvrns-ser.h>
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"

using json = nlohmann::json;

using namespace lbcrypto;

using std::cout, std::endl;

template <typename T>
void print_vec(const std::vector<T> &vec)
{
    for (const auto &el : vec)
    {
        std::cout << el << " ";
    }
    std::cout << std::endl;
}

int main()
{
    CCParams<CryptoContextBGVRNS> parameters;
    parameters.SetMultiplicativeDepth(1);
    parameters.SetPlaintextModulus(65537);
    parameters.SetScalingTechnique(FIXEDMANUAL);
    parameters.SetKeySwitchTechnique(
        KeySwitchTechnique::
            BV); // use BV instead of HYBRID, as it is a lot simpler to arithmetize, even if it requires a quadratic number of NTTs

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    // Enable features that you wish to use
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    std::cout << "reach 1" << std::endl;
    // generate keys and save private key in file
    KeyPair<DCRTPoly> keyPair;
    keyPair = cc->KeyGen();
    cc->EvalMultKeyGen(keyPair.secretKey);
    std::ofstream kof("privateKey.dat", std::ios::binary);
    Serial::Serialize(keyPair.secretKey, kof, SerType::BINARY);
    kof.close();

    json out_json;
    json c_arr = json::array();
    std::cout << "reach" << std::endl;

    // Generate some ciphertexts
    for (int i = 0; i < 5; ++i)
    {
        std::vector<int64_t> p_vec = {i + 1, i - 2, i + 3};
        std::cout << "p1: ";
        print_vec(p_vec);
        Plaintext p = cc->MakePackedPlaintext(p_vec);
        auto c = cc->Encrypt(keyPair.publicKey, p);

        std::ostringstream oss;
        Serial::Serialize(c, oss, SerType::BINARY);

        auto oss_str = oss.str();
        c_arr.push_back(base64::encode(reinterpret_cast<const unsigned char *>(oss_str.c_str()), oss_str.size()));
    }

    out_json["ciphertexts"] = c_arr;

    out_json["timestamp"] = std::time(0);

    std::ostringstream evos;
    cc->SerializeEvalMultKey(evos, SerType::BINARY);
    auto evos_str = evos.str();
    out_json["eval_mult_key"] = base64::encode(reinterpret_cast<const unsigned char *>(evos_str.c_str()), evos_str.size());

    std::ostringstream pubos;
    Serial::Serialize(keyPair.publicKey, pubos, SerType::BINARY);
    auto pubos_str = pubos.str();
    out_json["public_key"] = base64::encode(reinterpret_cast<const unsigned char *>(pubos_str.c_str()), pubos_str.size());

    // NOTE: hardcoding the expression that's known to be working correctly
    out_json["expression"] = "(0+1)*(0-1)";

    std::ofstream of("computation.json");
    of << out_json.dump(4);
    of.close();


    return 0;
}