#ifndef DIPLO_RPC_ROUTER_HPP
#define DIPLO_RPC_ROUTER_HPP

#include <vector>
#include "node/interface/i_node.hpp"
#include "msg/message.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

enum class RPCType
{
    Test = 0,
    Transaction,
    Computation,
    Output
};

class RPCRouter
{
public:
    RPCRouter(INode &node);

    json route(const json &json_msg);

private:
    INode &node_;
};

#endif