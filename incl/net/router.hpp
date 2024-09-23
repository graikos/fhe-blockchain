#ifndef DIPLO_ROUTER_HPP
#define DIPLO_ROUTER_HPP

#include <cstddef>
#include <vector>

#include "node/interface/i_node.hpp"
#include "msg/message.hpp"

class Router
{
public:
    Router(INode &node);

    std::vector<unsigned char> route(unsigned char *data, std::size_t n, MessageType mt, const std::string &peerID);

private:
    INode &node_;

};

#endif