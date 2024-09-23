#ifndef DIPLO_MESSAGE_HPP
#define DIPLO_MESSAGE_HPP

#include <utility>
#include "util/util.hpp"

enum class MessageType
{
    Hello = 0,
    GetAddr,
    Addr,
    Invalid,
    InvBlock,
    GetBlock,
    InfoBlock,
    InvTransaction,
    GetTransaction,
    InfoTransaction,
    InvComputation,
    GetComputation,
    InfoComputation,
    SyncBlock,
    SyncTransactions,
    ListTransactions,
    SyncComputations,
    ListComputations
};

namespace msg
{
    std::pair<int, MessageType> parse_header(unsigned char *header);
    void write_header(int len, int msg_type, std::vector<unsigned char> &vec);
};

#endif