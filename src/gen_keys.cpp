#include <iostream>

#include <fstream>

#include "nlohmann/json.hpp"

#include "sodium.h"
#include "base64.hpp"

using json = nlohmann::json;

int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        std::cout << "Invalid number of arguments passed." << std::endl;
        return 1;
    }

    const std::string fname(argv[1]);

    json secrets_json;

    secrets_json["ed25519"] = json::object();

    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(pk, sk);

    secrets_json["ed25519"]["secret_key"] = base64::encode(sk, crypto_sign_SECRETKEYBYTES);
    secrets_json["ed25519"]["public_key"] = base64::encode(pk, crypto_sign_PUBLICKEYBYTES);

    std::ofstream os(fname);
    os << secrets_json.dump(4) << std::endl;
    os.close();

    std::cout << "Successfully generated keys." << std::endl;

    return 0;
}