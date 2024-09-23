
#include <scheme/bgvrns/bgvrns-ser.h>
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include "ciphertext.h"
#include "cryptocontext.h"

#include <fstream>

#include "base64.hpp"

using namespace lbcrypto;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <secret key filename>" << std::endl;
        return 1;
    }
    std::cout << argv[1] << std::endl;

    std::ifstream kif(argv[1], std::ios::binary);
    if (!kif.is_open())
    {
        std::cerr << "Error: could not open file " << argv[1] << std::endl;
        return 1;
    }

    PrivateKey<DCRTPoly> privkey;
    Serial::Deserialize(privkey, kif, SerType::BINARY);
    kif.close();

    std::string output_str;
    std::getline(std::cin, output_str, '\0');

    auto dec_output = base64::decode(output_str);

    std::istringstream iss(dec_output);

    Ciphertext<DCRTPoly> cipher;
    Serial::Deserialize(cipher, iss, SerType::BINARY);

    auto cc = cipher->GetCryptoContext();
    Plaintext p;
    cc->Decrypt(privkey, cipher, &p);

    std::cout << p << std::endl;

    return 0;
}