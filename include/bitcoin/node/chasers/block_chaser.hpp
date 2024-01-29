/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_CHASERS_BLOCK_CHASER_HPP
#define LIBBITCOIN_NODE_CHASERS_BLOCK_CHASER_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down blocks for the candidate header chain.
/// Notify subscribers with gap closed event.
class BCN_API block_chaser
  : public network::reporter, protected network::tracker<block_chaser>
{
public:
    typedef uint64_t object_key;
    typedef network::desubscriber<object_key> subscriber;
    typedef subscriber::handler notifier;
    DELETE_COPY_MOVE(block_chaser);

    /// Construct an instance.
    /// -----------------------------------------------------------------------
    block_chaser(full_node& node) NOEXCEPT;
    ~block_chaser() NOEXCEPT;

    /// Start/stop.
    /// -----------------------------------------------------------------------
    void start(network::result_handler&& handler) NOEXCEPT;
    void stop() NOEXCEPT;

    /// Subscriptions.
    /// -----------------------------------------------------------------------
    object_key subscribe(notifier&& handler) NOEXCEPT;
    bool notify(object_key key) NOEXCEPT;

    /// Properties.
    /// -----------------------------------------------------------------------
    bool stopped() const NOEXCEPT;
    bool stranded() const NOEXCEPT;

private:
    object_key create_key() NOEXCEPT;
    void do_stop() NOEXCEPT;

    // These are thread safe (mostly).
    full_node& node_;
    network::asio::strand strand_;
    std::atomic_bool stopped_{ true };

    // These are not thread safe.
    object_key keys_{};
    subscriber subscriber_;
    network::deadline::ptr timer_;
};

} // namespace node
} // namespace libbitcoin

#endif
