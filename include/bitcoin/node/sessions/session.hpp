/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_SESSION_HPP
#define LIBBITCOIN_NODE_SESSION_HPP

#include <memory>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/protocols/protocol_header_in.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Session base class template.
/// This derives a session from a specialized session to change protocols.
/// Construction is from full_node, which derives from p2p, passed to base.
template <class Session>
class session
  : public Session
{
public:
    typedef std::shared_ptr<Session> ptr;

    /// Session attachment passes p2p, key and variable args.
    /// To avoid templatizing on p2p, pass node as first arg.
    session(network::p2p& network, size_t key, full_node& node)
      : Session(network, key), node_(node)
    {
    };

protected:
    typedef network::channel::ptr channel_ptr;

    void attach_protocols(const channel_ptr& channel) const NOEXCEPT override
    {
        const auto& self = *this;
        const auto version = channel->negotiated_version();

        // protocol_reject_70002
        // protocol_address_31402
        // protocol_ping_31402/60001
        Session::attach_protocols(channel);

        if (version >= network::messages::level::headers_protocol)
            channel->attach<protocol_header_in>(self, node_)->start();

        ////channel->attach<protocol_block_in>(self)->start();
        ////channel->attach<protocol_block_out>(self)->start();
        ////channel->attach<protocol_transaction_in>(self)->start();
        ////channel->attach<protocol_transaction_out>(self)->start();
    }

private:
    full_node& node_;
};

} // namespace node
} // namespace libbitcoin

#endif
