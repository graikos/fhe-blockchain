#include "net/rpc_router.hpp"

RPCRouter::RPCRouter(INode &node) : node_(node)
{
}

json RPCRouter::route(const json &json_msg)
{
    RPCType rpcType = json_msg.at("type");

    json resp;

    switch (rpcType)
    {
    case RPCType::Test:

        std::cout << "Got Test RPC" << std::endl;
        resp["status"] = STATUS_OK;
        node_.rpc_handle_say_hello();

        break;

    case RPCType::Transaction:
    {
        std::cout << "Got Transaction RPC" << std::endl;
        node_.rpc_handle_transaction(json_msg, resp);

        break;
    }

    case RPCType::Computation:
    {
        std::cout << "Got Computation RPC" << std::endl;
        node_.rpc_handle_computation(json_msg, resp);

        break;
    }
    case RPCType::Output:
    {
        std::cout << "Got Output RPC" << std::endl;
        node_.rpc_handle_output(json_msg, resp);

        break;
    }

    default:
        throw std::invalid_argument("Unknown RPC type.");
    }

    return resp;
}