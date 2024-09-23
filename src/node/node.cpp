#include "node/node.hpp"
#include <iostream>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/signal_set.hpp>
#include <asio/write.hpp>

#include "util/util.hpp"

#include "msg/message.hpp"
#include "message.pb.h"

#include "computer/concrete_computation_factory.hpp"

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using asio::ip::tcp;
namespace this_coro = asio::this_coro;

Node::Node(json &config, asio::io_context &io_context, std::shared_ptr<IChainstate> cs, std::shared_ptr<IBlockStore> bs,
           std::shared_ptr<IMemPool> mp, std::shared_ptr<ICompStore> compstore)
    : io_context_(io_context), stop_flag_(std::make_shared<std::atomic<bool>>(false)), router_(std::make_unique<Router>(*this)), rpc_router_(std::make_unique<RPCRouter>(*this)), is_synced_(false)
{
    conn_manager_ = std::make_unique<ConnectionManager>(*router_, io_context_, config);
    rpc_server_ = std::make_unique<RPCServer>(*rpc_router_, io_context, config);
    // TODO: remove after testing with single miner
    if (conn_manager_->listening_port == 5000)
    {
        std::ifstream ifs("config/secrets.json");
        json secjson = json::parse(ifs);
        ifs.close();
        std::cout << secjson.dump(4) << std::endl;
        wallet_ = std::make_shared<Wallet>(secjson);
    }
    else
    {
        wallet_ = std::make_shared<Wallet>();
    }
    chain_manager_ = std::make_unique<ChainManager>(config, cs,
                                                    bs, mp,
                                                    compstore, stop_flag_, wallet_);
    bootstrap_from_config(config);
}

void Node::start()
{
    std::cout << "starting node" << std::endl;

    std::thread io_thread([this]()
                          {
                            try
                            {
                                asio::signal_set signals(io_context_, SIGINT, SIGTERM);
                                signals.async_wait([&](auto, auto)
                                                { io_context_.stop(); });

                                this->conn_manager_->setup();
                                this->rpc_server_->setup();

                                io_context_.run();
                            } catch(std::exception& exc){
                                std::cerr << "exception in io thread: " << exc.what() << std::endl;
                            } });

    std::cout << "Sleeping for 4 secs to give time to conn manager" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));
    start_sync();

    // start miner only after syncing
    std::unique_lock<std::mutex> lock(sync_mu_);
    sync_cv_.wait(lock, [this]
                  { return this->is_synced_; });
    lock.unlock();

    for (;;)
    {

        // TODO: Remove this testing code that will only allow 5000 to mine
        if (conn_manager_->listening_port != 5000)
        {
            break;
        }

        // reset flag;
        *stop_flag_ = false;
        // here start a thread to mine blocks
        std::thread mine_thread([this]()
                                { this->chain_manager_->start_mining(); });
        mine_thread.join();
        // here, either we have mined a new block, or a received one and the mining stopped
        // TODO: maybe here reject if just mined but also received, or maybe broadcast and fork
        if (chain_manager_->have_mined_block())
        {
            std::cout << "mined block: " << base64::encode(chain_manager_->get_mined_block()->header_->hash().data(), chain_manager_->get_mined_block()->header_->hash().size()) << std::endl;
            handle_add_valid_block(chain_manager_->get_mined_block());
            handle_mined_block(chain_manager_->get_mined_block());
        }
        else
        {
            std::cout << "mining paused" << std::endl;
        }
    }

    io_thread.join();
    std::cout << "joined" << std::endl;
}

void Node::bootstrap_from_config(const json &config)
{
    auto res = util::bootstrap_addr_from_json(config);

    auto msg = build_getaddr();
    for (const auto &ep : res)
    {
        try
        {
            auto resp = conn_manager_->send_to_peer(ep.first, ep.second, msg, true);
            router_->route(resp.data(), resp.size(), MessageType::Addr, "");
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error connecting to bootstrapping node: " << e.what() << std::endl;
        }
    }
    conn_manager_->print_outbound();
}

bool Node::is_synced()
{
    std::lock_guard<std::mutex> lg(sync_mu_);
    return is_synced_;
}

void Node::set_synced()
{
    std::lock_guard<std::mutex> lg(sync_mu_);
    is_synced_ = true;
    sync_cv_.notify_one();
}

void Node::start_sync()
{
    // get a peer (probably one of the bootstrapping peers at this point)
    try
    {
        sync_peer_ = conn_manager_->get_one_outbound();
    }
    catch (const std::out_of_range &e)
    {
        set_synced();
        return;
    }

    auto msg = build_sync_block(1);
    conn_manager_->async_send_to_peer(sync_peer_, msg);
}

void Node::sync_mempool()
{
    auto msg = build_sync_transactions();
    conn_manager_->async_send_to_peer(sync_peer_, msg);
}

/*

============
Builders
============

*/

std::vector<unsigned char> Node::build_getaddr()
{
    GetAddr ga;
    ga.set_listening_address(conn_manager_->listening_address);
    ga.set_listening_port(conn_manager_->listening_port);
    int len = ga.ByteSizeLong();
    std::vector<unsigned char> msg_vec(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::GetAddr), msg_vec);
    if (!ga.SerializeToArray(msg_vec.data() + 8, len))
    {
        throw std::runtime_error("Could not serialize GetAddr message");
    }

    return msg_vec;
}

std::vector<unsigned char> Node::build_hello()
{
    HelloMsg hm;
    hm.set_greet("Hello");
    hm.set_id(1312);
    int len = hm.ByteSizeLong();
    std::vector<unsigned char> msg_vec(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::Hello), msg_vec);
    if (!hm.SerializeToArray(msg_vec.data() + 8, len))
    {
        throw std::runtime_error("Could not serialize GetAddr message");
    }

    return msg_vec;
}

std::vector<unsigned char> Node::build_invalid()
{
    Invalid inv;
    int len = inv.ByteSizeLong();
    std::vector<unsigned char> msg_vec(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::Invalid), msg_vec);
    if (!inv.SerializeToArray(msg_vec.data() + 8, len))
    {
        throw std::runtime_error("Could not serialize GetAddr message");
    }

    return msg_vec;
}

std::vector<unsigned char> Node::build_inv_block(const std::vector<unsigned char> &block_hash)
{
    InvBlock inv;
    inv.set_block_hash(std::string(block_hash.begin(), block_hash.end()));
    int len = inv.ByteSizeLong();
    std::vector<unsigned char> msg_vec(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::InvBlock), msg_vec);
    if (!inv.SerializeToArray(msg_vec.data() + 8, len))
    {
        throw std::runtime_error("Could not serialize InvBlock message");
    }

    return msg_vec;
}

std::vector<unsigned char> Node::build_inv_tx(const std::vector<unsigned char> &txid)
{
    InvTransaction inv;
    inv.set_txid(std::string(txid.begin(), txid.end()));
    int len = inv.ByteSizeLong();
    std::vector<unsigned char> msg_vec(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::InvTransaction), msg_vec);
    if (!inv.SerializeToArray(msg_vec.data() + 8, len))
    {
        throw std::runtime_error("Could not serialize InvTransaction message");
    }

    return msg_vec;
}

std::vector<unsigned char> Node::build_sync_transactions()
{
    SyncTransactions st;
    int len = st.ByteSizeLong();
    std::vector<unsigned char> msg_vec(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::SyncTransactions), msg_vec);
    if (!st.SerializeToArray(msg_vec.data() + 8, len))
    {
        throw std::runtime_error("Could not serialize SyncTransactions message");
    }

    return msg_vec;
}

std::vector<unsigned char> Node::build_inv_computation(const std::vector<unsigned char> &comp_hash)
{
    InvComputation inv;
    inv.set_comp_hash(std::string(comp_hash.begin(), comp_hash.end()));
    int len = inv.ByteSizeLong();
    std::vector<unsigned char> msg_vec(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::InvComputation), msg_vec);
    if (!inv.SerializeToArray(msg_vec.data() + 8, len))
    {
        throw std::runtime_error("Could not serialize InvComputation message");
    }

    return msg_vec;
}

std::vector<unsigned char> Node::build_sync_block(uint32_t height)
{
    SyncBlock sb;
    sb.set_height(height);
    int len = sb.ByteSizeLong();
    std::vector<unsigned char> msg_vec(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::SyncBlock), msg_vec);
    if (!sb.SerializeToArray(msg_vec.data() + 8, len))
    {
        throw std::runtime_error("Could not serialize SyncBlock message");
    }

    return msg_vec;
}

std::vector<unsigned char> Node::build_get_tx(const std::vector<unsigned char> &txid)
{
    // transaction does not exist
    GetTransaction gt;
    gt.set_txid(std::string(txid.begin(), txid.end()));

    int len = gt.ByteSizeLong();
    std::vector<unsigned char> msg_vec(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::GetTransaction), msg_vec);

    if (!gt.SerializeToArray(msg_vec.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize GetTransaction response.");
    }
    return msg_vec;
}

/*

============
Handlers
============

*/

// When receiving the GetAddr message, the node will update the inbound peer entry
// and will replace the address data with the listening address, instead of its outgoing.
// The underlying socket will not be changed, however by changing this info, the peer can now be shared
// and reached by others.
// However, there will be a small period from receiving the connection to actually reading the GetAddr message
// where the peer entry will have the outgoing address info. In the worst case where the peer is shared in this period,
// the peer won't be reached.
std::vector<unsigned char> Node::handle_get_peer_addrs(GetAddr &ga, const std::string &peerID)
{
    std::string peer_l_addr = ga.listening_address();
    int peer_l_port = ga.listening_port();

    conn_manager_->update_inb_peer_addr_info(peerID, peer_l_addr, peer_l_port);

    auto peers = conn_manager_->collect_peers();
    Addr addrVec;
    for (const auto &p : peers)
    {
        AddrPair *addrPair = addrVec.add_addrpairs();
        addrPair->set_address(p.first);
        addrPair->set_port(p.second);
    }

    int len = addrVec.ByteSizeLong();
    int msg_type = static_cast<int>(MessageType::Addr);
    std::vector<unsigned char> ser(8 + len);
    msg::write_header(len, msg_type, ser);
    if (!addrVec.SerializeToArray(ser.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize Addr response.");
    }
    // DEBUG:
    conn_manager_->print_inbound();
    return ser;
}

void Node::handle_addr(Addr &addrVec)
{
    auto msg = build_getaddr();
    for (const auto &pair : addrVec.addrpairs())
    {
        // if outbound map full or peer is actually self, move on to next peer
        if (conn_manager_->outbound_full())
        {
            break;
        }
        if (pair.address() == conn_manager_->listening_address && pair.port() == conn_manager_->listening_port)
        {
            continue;
        }
        if (!conn_manager_->peer_exists(pair.address(), pair.port()))
        {
            try
            {
                conn_manager_->async_send_to_peer(pair.address(), pair.port(), msg);
            }
            catch (std::exception &exc)
            {
                std::cerr << "Could not connect to received peer with addres: " << pair.address() << ":" << pair.port() << std::endl;
            }
        }
    }
}

void Node::handle_add_valid_block(std::shared_ptr<Block> block)
{
    auto good = chain_manager_->add_block(block, true);
    std::cout << "Block added? " << good << std::endl;
}

std::vector<unsigned char> Node::handle_inv_block(const InvBlock &msg)
{
    if (chain_manager_->block_exists(std::vector<unsigned char>(msg.block_hash().begin(), msg.block_hash().end())))
    {
        // block already exists, don't respond
        return std::vector<unsigned char>();
    }

    // block does not exist
    GetBlock gb;
    gb.set_block_hash(msg.block_hash());

    int len = gb.ByteSizeLong();
    std::vector<unsigned char> resp(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::GetBlock), resp);

    if (!gb.SerializeToArray(resp.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize GetBlock response.");
    }
    return resp;
}

std::vector<unsigned char> Node::handle_get_block(const GetBlock &msg)
{

    std::vector<unsigned char> blockHash(msg.block_hash().begin(), msg.block_hash().end());

    if (!chain_manager_->block_exists(blockHash))
    {
        // don't have requested block
        return std::vector<unsigned char>();
    }

    auto block = chain_manager_->get_block(blockHash);

    auto protoBlock = new ProtoBlock(std::move(block->to_proto()));

    InfoBlock ib;
    ib.set_allocated_block(protoBlock); // it's ok, since serialization will happen here, so protoBlock won't go out of scope

    int len = ib.ByteSizeLong();
    std::vector<unsigned char> resp(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::InfoBlock), resp);

    if (!ib.SerializeToArray(resp.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize InfoBlock response.");
    }
    return resp;
}

std::vector<unsigned char> Node::handle_info_block(const InfoBlock &msg)
{
    std::vector<unsigned char> resp;

    ProtoBlock protoblock = msg.block();
    auto block = std::make_shared<Block>(Block::from_proto(protoblock, ConcreteComputationFactory()));

    // InfoBlock means we asked for the block, but flooding could mean we
    // are receiving it more than once. Check blockstore nevertheless
    if (chain_manager_->block_exists(block->hash()))
    {
        return resp;
    }

    auto added = chain_manager_->add_block(block);
    std::cout << "Added? " << added << std::endl;

    if (added)
    {
        conn_manager_->async_broadcast(build_inv_block(block->hash()));
    }

    return resp;
}

std::vector<unsigned char> Node::handle_info_block_sync(const InfoBlock &msg)
{
    std::vector<unsigned char> resp;

    // if last we asked for is out of range, node is synced
    if (msg.out_of_range())
    {
        // TODO: here maybe initiate mempool sync before that
        set_synced();

        sync_mempool();

        return resp;
    }

    ProtoBlock protoblock = msg.block();
    auto block = std::make_shared<Block>(Block::from_proto(protoblock, ConcreteComputationFactory()));

    // InfoBlock means we asked for the block, but flooding could mean we
    // are receiving it more than once. Check blockstore nevertheless
    if (chain_manager_->block_exists(block->hash()))
    {
        return resp;
    }

    auto added = chain_manager_->add_block(block);
    std::cout << "Added (sync)? " << added << std::endl;

    if (added)
    {
        // still not synced, ask for next block
        auto msg = build_sync_block(chain_manager_->main_chain_->current_height() + 1);
        conn_manager_->async_send_to_peer(sync_peer_, msg);
    }

    return resp;
}

std::vector<unsigned char> Node::handle_inv_tx(const InvTransaction &msg)
{
    if (chain_manager_->tx_exists(std::vector<unsigned char>(msg.txid().begin(), msg.txid().end())))
    {
        // tx already exists
        return std::vector<unsigned char>();
    }

    // transaction does not exist
    GetTransaction gt;
    gt.set_txid(msg.txid());

    int len = gt.ByteSizeLong();
    std::vector<unsigned char> resp(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::GetTransaction), resp);

    if (!gt.SerializeToArray(resp.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize GetTransaction response.");
    }
    return resp;
}

std::vector<unsigned char> Node::handle_get_tx(const GetTransaction &msg)
{

    std::vector<unsigned char> txid(msg.txid().begin(), msg.txid().end());

    if (!chain_manager_->tx_exists(txid))
    {
        std::cout << "this transaction does not exist" << std::endl;
        // don't have requested transaction
        return std::vector<unsigned char>();
    }

    auto tx = chain_manager_->get_tx(txid);
    auto prototx = new ProtoTransaction(std::move(tx->to_proto()));

    InfoTransaction it;
    it.set_allocated_tx(prototx); // it's ok, since serialization will happen here, so prototx won't go out of scope

    int len = it.ByteSizeLong();
    std::vector<unsigned char> resp(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::InfoTransaction), resp);

    if (!it.SerializeToArray(resp.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize InfoTransaction response.");
    }
    return resp;
}

std::vector<unsigned char> Node::handle_info_tx(const InfoTransaction &msg)
{
    std::vector<unsigned char> resp;

    ProtoTransaction prototx = msg.tx();
    auto tx = std::make_shared<Transaction>(Transaction::from_proto(prototx, false));

    if (chain_manager_->tx_exists(tx->TXID()))
    {
        // last ditch check that transaction does not already exist
        return resp;
    }

    bool added = chain_manager_->add_tx(tx);

    // if TX was added successfully, propagate
    if (added)
    {
        std::cout << "Added transaction to mempool, now broadcast" << std::endl;
        conn_manager_->async_broadcast(build_inv_tx(tx->TXID()));
    }

    return resp;
}

std::vector<unsigned char> Node::handle_inv_computation(const InvComputation &msg)
{

    if (chain_manager_->computation_exists(std::vector<unsigned char>(msg.comp_hash().begin(), msg.comp_hash().end())))
    {
        // computation alread exists
        return std::vector<unsigned char>();
    }

    // computation does not exist
    GetComputation gc;
    gc.set_comp_hash(msg.comp_hash());

    int len = gc.ByteSizeLong();
    std::vector<unsigned char> resp(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::GetComputation), resp);

    if (!gc.SerializeToArray(resp.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize GetComputation response.");
    }
    return resp;
}

std::vector<unsigned char> Node::handle_get_computation(const GetComputation &msg)
{
    std::vector<unsigned char> comp_hash(msg.comp_hash().begin(), msg.comp_hash().end());

    if (!chain_manager_->computation_exists(comp_hash))
    {
        std::cout << "this computation does not exist" << std::endl;
        // don't have requested computation
        return std::vector<unsigned char>();
    }

    auto comp = chain_manager_->get_computation(comp_hash);
    auto protocomp = new ProtoComputation(std::move(comp->to_proto()));

    InfoComputation ic;
    ic.set_allocated_comp(protocomp); // it's ok, since serialization will happen here, so prototx won't go out of scope

    int len = ic.ByteSizeLong();
    std::vector<unsigned char> resp(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::InfoComputation), resp);

    if (!ic.SerializeToArray(resp.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize InfoComputation response.");
    }
    return resp;
}

std::vector<unsigned char> Node::handle_info_computation(const InfoComputation &msg)
{
    std::vector<unsigned char> resp;

    ProtoComputation protocomp = msg.comp();
    auto factory = ConcreteComputationFactory();

    auto comp = factory.createComputation(protocomp);

    if (chain_manager_->computation_exists(comp->hash()))
    {
        // last ditch check that transaction does not already exist
        return resp;
    }

    bool added = chain_manager_->add_computation(comp);

    // if TX was added successfully, propagate
    if (added)
    {
        std::cout << "Added computation to computation store, now broadcast" << std::endl;
        conn_manager_->async_broadcast(build_inv_computation(comp->hash()));
    }

    return resp;
}

void Node::handle_mined_block(std::shared_ptr<Block> mined_block)
{
    // here, we have mined a block and added to local chain
    // we need to broadcast
    conn_manager_->async_broadcast(build_inv_block(mined_block->hash()));
}

std::vector<unsigned char> Node::handle_sync_block(const SyncBlock &msg)
{
    uint32_t height_req = msg.height();

    auto out_of_range = (height_req > chain_manager_->main_chain_->current_height());

    InfoBlock ib;
    if (!out_of_range)
    {
        auto block_req_header = chain_manager_->main_chain_->get_header(height_req);
        auto block = chain_manager_->get_block(block_req_header->hash());

        auto protoBlock = new ProtoBlock(std::move(block->to_proto()));
        ib.set_allocated_block(protoBlock); // it's ok, since serialization will happen here, so protoBlock won't go out of scope
        ib.set_out_of_range(false);
    }
    else
    {
        ib.set_out_of_range(true);
    }

    int len = ib.ByteSizeLong();
    std::vector<unsigned char> resp(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::InfoBlock), resp);

    if (!ib.SerializeToArray(resp.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize InfoBlock response.");
    }
    return resp;
}

std::vector<unsigned char> Node::handle_sync_transactions(const SyncTransactions &msg)
{
    ListTransactions lt;
    auto txid_list = chain_manager_->mempool_list_txid();

    auto txids_field = lt.mutable_txids();

    for (auto &txid : txid_list)
    {
        txids_field->Add(std::move(txid));
    }

    int len = lt.ByteSizeLong();
    std::vector<unsigned char> resp(8 + len);
    msg::write_header(len, static_cast<int>(MessageType::ListTransactions), resp);

    if (!lt.SerializeToArray(resp.data() + 8, len))
    {
        throw std::runtime_error("Failed to serialize ListTransactions response.");
    }

    return resp;
}

std::vector<unsigned char> Node::handle_list_transactions(const ListTransactions &msg)
{
    for (int i = 0; i < msg.txids_size(); ++i)
    {
        const std::string &txid = msg.txids(i);

        // for every txid in the list, broadcast and ask peers
        auto msg = build_get_tx(std::vector<unsigned char>(txid.begin(), txid.end()));
        conn_manager_->async_broadcast(msg);
    }

    return std::vector<unsigned char>();
}

/**
 *
 *
 * ==================================
 *
 * RPC HANDLERS
 *
 * ==================================
 *
 *
 *
 */

void Node::rpc_handle_say_hello()
{
    auto msg = build_hello();
    conn_manager_->async_broadcast(msg);
}

void Node::rpc_handle_transaction(const json &msg, json &resp)
{

    std::vector<unsigned char> rec_pub_key;
    uint64_t amount;
    uint64_t fee;

    try
    {
        auto p = base64::decode(msg.at("recipient_public_key"));
        rec_pub_key.assign(p.begin(), p.end());
        amount = msg.at("amount");
        fee = msg.at("fee");
    }
    catch (const json::out_of_range &e)
    {
        resp["status"] = STATUS_BAD_REQUEST;
        std::cout << e.what() << std::endl;
        return;
    }

    std::cout << "about to create transaction from wallet" << std::endl;

    std::shared_ptr<Transaction> tx;
    try
    {
        tx = wallet_->new_transaction(rec_pub_key, amount, fee);
    }
    catch (const std::runtime_error &e)
    {
        resp["status"] = STATUS_PAYMENT_REQUIRED;
        return;
    }

    // add to local mem pool before broadcasting
    chain_manager_->add_tx(tx);

    auto invtx = build_inv_tx(tx->TXID());
    conn_manager_->async_broadcast(invtx);

    resp["status"] = STATUS_OK;
}

void Node::rpc_handle_computation(const json &req, json &resp)
{
    std::shared_ptr<Computation> comp;
    try
    {
        comp = std::make_shared<FHEComputer>(req);
    }
    catch (const std::exception &e)
    {
        resp["status"] = STATUS_INTERNAL_SERVER_ERROR;
        std::cerr << e.what() << '\n';
        return;
    }

    std::cout << "about to add computation" << std::endl;
    bool added = chain_manager_->add_computation(comp);
    if (added)
    {
        std::cout << "RPC: added computation to store" << std::endl;
    }

    std::cout << "about to build invcomp for broadcast" << std::endl;
    auto invcomp = build_inv_computation(comp->hash());
    conn_manager_->async_broadcast(invcomp);

    resp["status"] = STATUS_OK;
}

void Node::rpc_handle_output(const json &req, json &resp)
{
    try
    {
        uint32_t height = req.at("block_height");
        auto header = chain_manager_->main_chain_->get_header(height);

        auto output = header->computations_.at(req.at("computation_index"))->output();

        auto enc_output = base64::encode(output.data(), output.size());
        resp["status"] = STATUS_OK;
        resp["output"] = enc_output;
    }
    catch (const json::out_of_range &e)
    {
        resp["status"] = STATUS_BAD_REQUEST;
    }
    catch (const std::out_of_range &e)
    {
        resp["status"] = STATUS_NOT_FOUND;
    }
    catch (const std::exception &e)
    {
        resp["status"] = STATUS_INTERNAL_SERVER_ERROR;
    }
}