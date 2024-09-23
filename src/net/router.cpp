#include "net/router.hpp"
#include <iostream>
#include <vector>
#include "message.pb.h"

Router::Router(INode &node) : node_(node)
{
}

std::vector<unsigned char> Router::route(unsigned char *data, std::size_t n, MessageType mt, const std::string &peerID)
{

    std::vector<unsigned char> res;

    switch (mt)
    {
    case MessageType::Hello:
    {
        HelloMsg hm;
        hm.ParseFromArray(data, n);
        std::cout << "caught hello" << std::endl;
        std::cout << hm.greet() << std::endl;
        res = node_.build_invalid();
        break;
    }
    case MessageType::GetAddr:
    {
        std::cout << "Caught GetAddr" << std::endl;
        GetAddr ga;
        ga.ParseFromArray(data, n);
        res = node_.handle_get_peer_addrs(ga, peerID);
        break;
    }
    case MessageType::Addr:
    {
        std::cout << "Caught Addr" << std::endl;
        Addr addrVec;
        addrVec.ParseFromArray(data, n);
        node_.handle_addr(addrVec);
        break;
    }
    case MessageType::InvBlock:
    {
        std::cout << "Caught InvBlock" << std::endl;
        InvBlock invBlock;
        invBlock.ParseFromArray(data, n);
        res = node_.handle_inv_block(invBlock);
        break;
    }
    case MessageType::GetBlock:
    {
        std::cout << "Caught GetBlock" << std::endl;
        GetBlock getBlock;
        getBlock.ParseFromArray(data, n);
        res = node_.handle_get_block(getBlock);
        break;
    }
    case MessageType::InfoBlock:
    {
        std::cout << "Caught InfoBlock" << std::endl;
        InfoBlock infoBlock;
        infoBlock.ParseFromArray(data, n);
        if (node_.is_synced())
        {
            res = node_.handle_info_block(infoBlock);
        }
        else
        {
            res = node_.handle_info_block_sync(infoBlock);
        }
        break;
    }
    case MessageType::SyncBlock:
    {
        std::cout << "Caught SyncBlock" << std::endl;
        SyncBlock syncBlock;
        syncBlock.ParseFromArray(data, n);
        res = node_.handle_sync_block(syncBlock);
        break;
    }
    case MessageType::InvTransaction:
    {
        std::cout << "Caught InvTransaction" << std::endl;
        InvTransaction invTransaction;
        invTransaction.ParseFromArray(data, n);
        res = node_.handle_inv_tx(invTransaction);
        break;
    }
    case MessageType::GetTransaction:
    {
        std::cout << "Caught GetTransaction" << std::endl;
        GetTransaction getTransaction;
        getTransaction.ParseFromArray(data, n);
        res = node_.handle_get_tx(getTransaction);
        break;
    }
    case MessageType::InfoTransaction:
    {
        std::cout << "Caught InfoTransaction" << std::endl;
        InfoTransaction infoTransaction;
        infoTransaction.ParseFromArray(data, n);
        res = node_.handle_info_tx(infoTransaction);
        break;
    }
    case MessageType::InvComputation:
    {
        std::cout << "Caught InvComputation" << std::endl;
        InvComputation invComputation;
        invComputation.ParseFromArray(data, n);
        res = node_.handle_inv_computation(invComputation);
        break;
    }
    case MessageType::GetComputation:
    {
        std::cout << "Caught GetComputation" << std::endl;
        GetComputation getComputation;
        getComputation.ParseFromArray(data, n);
        res = node_.handle_get_computation(getComputation);
        break;
    }
    case MessageType::InfoComputation:
    {
        std::cout << "Caught InfoComputation" << std::endl;
        InfoComputation infoComputation;
        infoComputation.ParseFromArray(data, n);
        res = node_.handle_info_computation(infoComputation);
        break;
    }
    case MessageType::SyncTransactions:
    {
        std::cout << "Caught SyncTransactions" << std::endl;
        SyncTransactions syncTransactions;
        syncTransactions.ParseFromArray(data, n);
        res = node_.handle_sync_transactions(syncTransactions);
        break;
    }
    case MessageType::ListTransactions:
    {
        std::cout << "Caught ListTransactions" << std::endl;
        ListTransactions listTransactions;
        listTransactions.ParseFromArray(data, n);
        res = node_.handle_list_transactions(listTransactions);
        break;
    }
    default:
        std::cout << "defaul case caught" << std::endl;
    }

    return res;
}