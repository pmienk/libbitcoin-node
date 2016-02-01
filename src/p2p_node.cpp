/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/p2p_node.hpp>

#include <functional>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/session_block_sync.hpp>
#include <bitcoin/node/session_header_sync.hpp>

namespace libbitcoin {
namespace node {

using namespace bc::blockchain;
using namespace bc::chain;
using namespace bc::config;
using namespace bc::network;
using std::placeholders::_1;
using std::placeholders::_2;

static const configuration default_configuration()
{
    configuration defaults;

    defaults.network.threads = NETWORK_THREADS;
    defaults.network.identifier = NETWORK_IDENTIFIER_MAINNET;
    defaults.network.inbound_port = NETWORK_INBOUND_PORT_MAINNET;
    defaults.network.connection_limit = NETWORK_CONNECTION_LIMIT;
    defaults.network.outbound_connections = NETWORK_OUTBOUND_CONNECTIONS;
    defaults.network.manual_retry_limit = NETWORK_MANUAL_RETRY_LIMIT;
    defaults.network.connect_batch_size = NETWORK_CONNECT_BATCH_SIZE;
    defaults.network.connect_timeout_seconds = NETWORK_CONNECT_TIMEOUT_SECONDS;
    defaults.network.channel_handshake_seconds = NETWORK_CHANNEL_HANDSHAKE_SECONDS;
    defaults.network.channel_poll_seconds = NETWORK_CHANNEL_POLL_SECONDS;
    defaults.network.channel_heartbeat_minutes = NETWORK_CHANNEL_HEARTBEAT_MINUTES;
    defaults.network.channel_inactivity_minutes = NETWORK_CHANNEL_INACTIVITY_MINUTES;
    defaults.network.channel_expiration_minutes = NETWORK_CHANNEL_EXPIRATION_MINUTES;
    defaults.network.channel_germination_seconds = NETWORK_CHANNEL_GERMINATION_SECONDS;
    defaults.network.host_pool_capacity = NETWORK_HOST_POOL_CAPACITY;
    defaults.network.relay_transactions = NETWORK_RELAY_TRANSACTIONS;
    defaults.network.hosts_file = NETWORK_HOSTS_FILE;
    defaults.network.debug_file = NETWORK_DEBUG_FILE;
    defaults.network.error_file = NETWORK_ERROR_FILE;
    defaults.network.self = NETWORK_SELF;
    defaults.network.blacklists = NETWORK_BLACKLISTS;
    defaults.network.seeds = NETWORK_SEEDS_MAINNET;

    defaults.chain.threads = BLOCKCHAIN_THREADS;
    defaults.chain.history_start_height = BLOCKCHAIN_HISTORY_START_HEIGHT;
    defaults.chain.block_pool_capacity = BLOCKCHAIN_BLOCK_POOL_CAPACITY;
    defaults.chain.transaction_pool_capacity = BLOCKCHAIN_TRANSACTION_POOL_CAPACITY;
    defaults.chain.transaction_pool_consistency = BLOCKCHAIN_TRANSACTION_POOL_CONSISTENCY;
    defaults.chain.use_testnet_rules = BLOCKCHAIN_TESTNET_RULES_MAINNET;
    defaults.chain.database_path = BLOCKCHAIN_DATABASE_PATH;
    defaults.chain.checkpoints = BLOCKCHAIN_CHECKPOINTS_MAINNET;

    defaults.node.threads = NODE_THREADS;
    defaults.node.quorum = NODE_QUORUM;
    defaults.node.blocks_per_second = NODE_BLOCKS_PER_SECOND;
    defaults.node.headers_per_second = NODE_HEADERS_PER_SECOND;
    defaults.node.peers = NODE_PEERS;

    return defaults;
};

const configuration p2p_node::defaults = default_configuration();

///////////////////////////////////////////////////////////////////////////////
// TODO: move blockchain threadpool into blockchain and transaction pool start.
// This requires that the blockchain components support start, stop and close.
///////////////////////////////////////////////////////////////////////////////

p2p_node::p2p_node(const configuration& configuration)
  : p2p(configuration.network),
    configuration_(configuration),
    blockchain_threadpool_(0),
    blockchain_(blockchain_threadpool_, configuration.chain),
    transaction_pool_(blockchain_threadpool_, blockchain_, configuration.chain)
{
}

// Properties.
// ----------------------------------------------------------------------------

block_chain& p2p_node::query()
{
    return blockchain_;
}

transaction_pool& p2p_node::pool()
{
    return transaction_pool_;
}

// Start sequence.
// ----------------------------------------------------------------------------

void p2p_node::start(result_handler handler)
{
    if (!stopped())
    {
        handler(error::operation_failed);
        return;
    }

    blockchain_threadpool_.join();
    blockchain_threadpool_.spawn(configuration_.chain.threads,
        thread_priority::low);

    blockchain_.start(
        std::bind(&p2p_node::handle_blockchain_start,
            this, _1, handler));
}

void p2p_node::handle_blockchain_start(const code& ec, result_handler handler)
{
    if (ec)
    {
        log::info(LOG_NODE)
            << "Blockchain failed to start: " << ec.message();
        handler(ec);
        return;
    }

    blockchain_.fetch_last_height(
        std::bind(&p2p_node::handle_fetch_height,
            this, _1, _2, handler));
}

void p2p_node::handle_fetch_height(const code& ec, uint64_t height,
    result_handler handler)
{
    if (ec)
    {
        log::error(LOG_SESSION)
            << "Failure fetching blockchain start height: " << ec.message();
        handler(ec);
        return;
    }

    set_height(height);

    // This is the end of the derived start sequence.
    // Stopped is true and no network threads until after this call.
    p2p::start(handler);
}

// Run sequence.
// ----------------------------------------------------------------------------

void p2p_node::run(result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Ensure consistency in the case where member height is changing.
    const auto current_height = height();

    blockchain_.fetch_block_header(current_height,
        std::bind(&p2p_node::handle_fetch_header,
            this, _1, _2, current_height, handler));
}

void p2p_node::handle_fetch_header(const code& ec, const header& block_header,
    size_t block_height, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_SESSION)
            << "Failure fetching blockchain start header: " << ec.message();
        handler(ec);
        return;
    }

    const config::checkpoint top(block_header.hash(), block_height);

    // The instance is retained by the stop handler (i.e. until shutdown).
    attach<session_header_sync>(hashes_, top, configuration_)->start(
        std::bind(&p2p_node::handle_headers_synchronized,
            this, _1, block_height, handler));
}

void p2p_node::handle_headers_synchronized(const code& ec, size_t block_height,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Failure synchronizing headers: " << ec.message();
        handler(ec);
        return;
    }

    if (hashes_.empty())
    {
        log::info(LOG_NETWORK)
            << "Completed header synchronization.";
        handle_blocks_synchronized(error::success, block_height, handler);
        return;
    }

    // First height in hew headers.
    const auto start_height = block_height + 1;
    const auto end_height = start_height + hashes_.size();

    log::info(LOG_NETWORK)
        << "Completed header synchronization [" << start_height << "-"
        << end_height << "]";

    // The instance is retained by the stop handler (i.e. until shutdown).
    attach<session_block_sync>(hashes_, start_height, configuration_)->start(
        std::bind(&p2p_node::handle_blocks_synchronized,
            this, _1, start_height, handler));
}

void p2p_node::handle_blocks_synchronized(const code& ec, size_t start_height,
    result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Failure synchronizing blocks: " << ec.message();
        handler(ec);
        return;
    }

    log::info(LOG_NETWORK)
        << "Completed block synchronization [" << start_height
        << "-" << height() << "]";

    // This is the end of the derived run sequence.
    p2p::run(handler);
}

// Stop sequence.
// ----------------------------------------------------------------------------

void p2p_node::subscribe_blockchain(reorganize_handler handler)
{
    query().subscribe_reorganize(handler);

}

void p2p_node::subscribe_transaction_pool(transaction_handler handler)
{
    pool().subscribe_transaction(handler);
}

// Stop sequence.
// ----------------------------------------------------------------------------

void p2p_node::stop(result_handler handler)
{
    blockchain_.stop();
    transaction_pool_.stop();
    blockchain_threadpool_.shutdown();

    // This is the end of the derived stop sequence.
    p2p::stop(handler);
}

// Destruct sequence.
// ----------------------------------------------------------------------------

void p2p_node::close()
{
    p2p_node::stop(unhandled);

    blockchain_threadpool_.join();

    // This is the end of the destruct sequence.
    p2p::close();
}

p2p_node::~p2p_node()
{
    // A reference cycle cannot exist with this class, since we don't capture
    // shared pointers to it. Therefore this will always clear subscriptions.
    // This allows for shutdown based on destruct without need to call stop.
    p2p_node::close();
}

} // namspace node
} //namespace libbitcoin
