#include <openfhe.h>

#include <iostream>
#include <memory>
#include <thread>
#include <fstream>
#include <atomic>

#include <asio/co_spawn.hpp>
#include "message.pb.h"
#include "util/util.hpp"
#include "node/node.hpp"
#include <nlohmann/json.hpp>

#include "store/mem_blockstore.hpp"
#include "store/mem_chainstate.hpp"
#include "store/mem_pool.hpp"
#include "store/mem_compstore.hpp"

#include "computer/fhe_computer.hpp"

#include <scheme/bgvrns/bgvrns-ser.h>
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"

#include "base64.hpp"

using json = nlohmann::json;

using namespace lbcrypto;

std::vector<std::shared_ptr<FHEComputer>> generate_computations(int);

int main(int argc, char *argv[])
{

    const std::string tost = "config/config.json";
    asio::io_context io_context(1);

    std::ifstream f(tost);
    if (!f.is_open())
    {
        throw std::invalid_argument("Could not open json file.");
    }
    json config_json;
    f >> config_json;
    f.close();

    int no_of_comps = 1;

    // override the config if command line options are passed
    if (argc == 2 || argc == 4 || argc == 5)
    {
        if (argc == 4 || argc == 5)
        {
            config_json["net"]["address"] = std::string(argv[1]);
            std::cout << config_json["net"]["address"] << std::endl;

            config_json["net"]["port"] = std::stoi(argv[2]);
            std::cout << config_json["net"]["port"] << std::endl;

            config_json["net"]["rpc_port"] = std::stoi(argv[3]);

            if (argc == 5)
            {
                no_of_comps = std::stoi(argv[4]);
            }
        }
        else if (argc == 2)
        {
            no_of_comps = std::stoi(argv[1]);
        }
    }

    auto chainstate = std::make_shared<MemChainstate>();
    auto blockstore = std::make_shared<MemBlockStore>();
    auto mempool = std::make_shared<MemPool>();
    auto compstore = std::make_shared<MemCompStore>();

    auto stop_flag = std::make_shared<std::atomic<bool>>(false);

    auto computations = generate_computations(no_of_comps);

    // remove this for now to test RPC

    // for (const auto &comp : computations)
    // {
    //     compstore->store_computation(comp);
    // }

    Node node(config_json, io_context, chainstate, blockstore, mempool, compstore);
    node.start();

    return 0;
}

std::vector<std::shared_ptr<FHEComputer>> generate_computations(int num_of_comps)
{

    std::vector<std::shared_ptr<FHEComputer>> res;

    for (int num = 0; num < num_of_comps; ++num)
    {
        // every comp will have a CryptoContext with its own unique ID
        // here generate a random 32bytes ID
        std::vector<unsigned char> uid(32);
        randombytes_buf(uid.data(), 32);

        CCParams<CryptoContextBGVRNS> parameters;
        parameters.SetUID(uid);
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
        // generate keys and save private key in file
        KeyPair<DCRTPoly> keyPair;
        keyPair = cc->KeyGen();
        cc->EvalMultKeyGen(keyPair.secretKey);

        std::string privfname = "privateKey_" + std::to_string(num) + ".dat";
        std::ofstream kof(privfname, std::ios::binary);
        Serial::Serialize(keyPair.secretKey, kof, SerType::BINARY);
        kof.close();

        json out_json;
        json c_arr = json::array();

        std::cout << "===================" << std::endl;
        std::cout << "Generating computation #" << num << ":" << std::endl;
        // Generate some ciphertexts
        for (int i = 0; i < 5; ++i)
        {
            std::vector<int64_t> p_vec = {(num+1)*(i + 1), (num+1)*(i - 2), (num+1)*(i + 3)};
            std::cout << "Plaintext " << i << ":" << std::endl;
            Plaintext p = cc->MakePackedPlaintext(p_vec);
            std::cout << p << std::endl;
            auto c = cc->Encrypt(keyPair.publicKey, p);

            std::ostringstream oss;
            Serial::Serialize(c, oss, SerType::BINARY);

            auto oss_str = oss.str();
            c_arr.push_back(base64::encode(reinterpret_cast<const unsigned char *>(oss_str.c_str()), oss_str.size()));
        }

        out_json["ciphertexts"] = c_arr;

        // out_json["timestamp"] = std::time(0);
        // instead of normal timestamp, use an index
        out_json["timestamp"] = num;

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

        // hardcoding here, but can be found in rpc_router.hpp
        out_json["type"] = 2;

        res.push_back(std::make_shared<FHEComputer>(out_json));

        std::string compfname = "computation_" + std::to_string(num) + ".json";
        std::ofstream of(compfname);
        of << out_json.dump(4);
        of.close();
    }
    return res;
}