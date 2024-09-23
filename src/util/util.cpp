#include "util/util.hpp"
#include <iostream>
#include <vector>
#include <utility>
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

int util::chars_to_int32_big_endian(const unsigned char *buffer)
{
    return int((buffer[0]) << 24 |
               (buffer[1]) << 16 |
               (buffer[2]) << 8 |
               (buffer[3]));
}

std::vector<unsigned char> util::int32_to_vector_big_endian(int num)
{
    std::vector<unsigned char> res(4);

    res[3] = num & 0xff;
    res[2] = (num >> 8) & 0xff;
    res[1] = (num >> 16) & 0xff;
    res[0] = (num >> 24) & 0xff;
    return res;
}

void util::int32_to_uchar_big_endian(int num, unsigned char *res)
{
    res[3] = num & 0xff;
    res[2] = (num >> 8) & 0xff;
    res[1] = (num >> 16) & 0xff;
    res[0] = (num >> 24) & 0xff;
}

uint32_t util::chars_to_uint32_big_endian(const unsigned char *buffer)
{
    return uint32_t((buffer[0] << 24) |
                    (buffer[1] << 16) |
                    (buffer[2] << 8) |
                    (buffer[3]));
}
std::vector<unsigned char> util::uint32_to_vector_big_endian(uint32_t num)
{
    std::vector<unsigned char> res(4);

    res[3] = num & 0xff;
    res[2] = (num >> 8) & 0xff;
    res[1] = (num >> 16) & 0xff;
    res[0] = (num >> 24) & 0xff;
    return res;
}
void util::uint32_to_uchar_big_endian(uint32_t num, unsigned char *res)
{
    res[3] = num & 0xff;
    res[2] = (num >> 8) & 0xff;
    res[1] = (num >> 16) & 0xff;
    res[0] = (num >> 24) & 0xff;
}

uint64_t util::chars_to_uint64_big_endian(const unsigned char *buffer)
{
    return uint64_t((uint64_t(buffer[0]) << 56) |
                    (uint64_t(buffer[1]) << 48) |
                    (uint64_t(buffer[2]) << 40) |
                    (uint64_t(buffer[3]) << 32) |
                    (uint64_t(buffer[4]) << 24) |
                    (uint64_t(buffer[5]) << 16) |
                    (uint64_t(buffer[6]) << 8) |
                    (uint64_t(buffer[7])));
}

std::vector<unsigned char> util::uint64_to_vector_big_endian(uint64_t num)
{
    std::vector<unsigned char> res(8);

    res[7] = num & 0xff;
    res[6] = (num >> 8) & 0xff;
    res[5] = (num >> 16) & 0xff;
    res[4] = (num >> 24) & 0xff;
    res[3] = (num >> 32) & 0xff;
    res[2] = (num >> 40) & 0xff;
    res[1] = (num >> 48) & 0xff;
    res[0] = (num >> 56) & 0xff;
    return res;
}

void util::uint64_to_uchar_big_endian(uint64_t num, unsigned char *res)
{
    res[7] = num & 0xff;
    res[6] = (num >> 8) & 0xff;
    res[5] = (num >> 16) & 0xff;
    res[4] = (num >> 24) & 0xff;
    res[3] = (num >> 32) & 0xff;
    res[2] = (num >> 40) & 0xff;
    res[1] = (num >> 48) & 0xff;
    res[0] = (num >> 56) & 0xff;
}
std::vector<std::pair<std::string, int>> util::ipv4_addr_list_from_txt(std::string &fname)
{
    std::ifstream f(fname);
    if (!f.is_open())
    {
        throw std::invalid_argument("Could not open json file.");
    }

    json json_addr;
    f >> json_addr;

    f.close();

    std::vector<std::pair<std::string, int>> res;

    for (const auto &addr : json_addr)
    {
        res.push_back(std::make_pair(addr[0], addr[1]));
    }

    return res;
}

std::vector<std::pair<std::string, int>> util::bootstrap_addr_from_json(const json &json_ob)
{
    std::vector<std::pair<std::string, int>> res;

    for (const auto &addr : json_ob["net"]["bootstrap"])
    {
        res.push_back(std::make_pair(addr[0], addr[1]));
    }

    return res;
}

std::string util::addr_and_port_to_str(const std::string &addr, int port)
{
    std::stringstream ss;
    ss << addr << ":" << port;
    return ss.str();
}

void util::print_hex(const std::vector<unsigned char> &vec)
{
    for (const unsigned char &byte : vec)
    {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout << std::dec << std::endl;
}

std::string util::txid_vout_pair_to_key(const std::vector<unsigned char> &txid, uint64_t vout)
{
    std::string res(txid.begin(), txid.end());
    auto voutser = uint64_to_vector_big_endian(vout);
    res.append(voutser.begin(), voutser.end());
    return res;
}