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
#include <bitcoin/node/chasers/chaser.hpp>

#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

using namespace network;
using namespace system::chain;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser::chaser(full_node& node) NOEXCEPT
  : node_(node),
    strand_(node.service().get_executor()),
    milestone_(node.config().bitcoin.milestone),
    checkpoints_(system::sort_copy(node.config().bitcoin.checkpoints)),
    reporter(node.log)
{
}

// Methods.
// ----------------------------------------------------------------------------

void chaser::stopping(const code&) NOEXCEPT
{
}

bool chaser::closed() const NOEXCEPT
{
    return node_.closed();
}

bool chaser::suspended() const NOEXCEPT
{
    return node_.suspended();
}

code chaser::fault(const code& ec) NOEXCEPT
{
    node_.fault(ec);
    return ec;
}

void chaser::resume() NOEXCEPT
{
    node_.resume();
}

code chaser::snapshot(const store::event_handler& handler) NOEXCEPT
{
    return node_.snapshot(handler);
}

code chaser::reload(const store::event_handler& handler) NOEXCEPT
{
    return node_.reload(handler);
}

// Events.
// ----------------------------------------------------------------------------

object_key chaser::subscribe_events(event_notifier&& handler) NOEXCEPT
{
    return node_.subscribe_events(std::move(handler));
}

void chaser::notify(const code& ec, chase event_,
    event_value value) const NOEXCEPT
{
    node_.notify(ec, event_, value);
}

// Properties.
// ----------------------------------------------------------------------------

const node::configuration& chaser::config() const NOEXCEPT
{
    return node_.config();
}

query& chaser::archive() const NOEXCEPT
{
    return node_.archive();
}

asio::strand& chaser::strand() NOEXCEPT
{
    return strand_;
}

bool chaser::stranded() const NOEXCEPT
{
    return strand_.running_in_this_thread();
}

bool chaser::is_current() const NOEXCEPT
{
    return node_.is_current();
}

bool chaser::is_current(uint32_t timestamp) const NOEXCEPT
{
    return node_.is_current(timestamp);
}

// Methods.
// ----------------------------------------------------------------------------

bool chaser::is_under_bypass(size_t height) const NOEXCEPT
{
    return is_under_checkpoint(height) || is_under_milestone(height);
}

// TODO: the existence of milestone could be cached/updated.
bool chaser::is_under_milestone(size_t height) const NOEXCEPT
{
    // get_header_key returns null_hash on not found.
    if (height > milestone_.height() || milestone_.hash() == system::null_hash)
        return false;

    const auto& query = archive();
    return query.get_header_key(query.to_candidate(milestone_.height())) ==
        milestone_.hash();
}

bool chaser::is_under_checkpoint(size_t height) const NOEXCEPT
{
    // Checkpoints are sorted by increasing height (optimal).
    return system::chain::checkpoint::is_under(checkpoints_, height);
}

size_t chaser::top_checkpoint() const NOEXCEPT
{
    // Checkpoints are sorted by increasing height (required).
    return checkpoints_.empty() ? zero : checkpoints_.back().height();
}

// Position.
// ----------------------------------------------------------------------------

void chaser::set_position(size_t height) NOEXCEPT
{
    position_ = height;
}

size_t& chaser::position() NOEXCEPT
{
    return position_;
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
