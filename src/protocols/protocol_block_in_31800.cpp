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
#include <bitcoin/node/protocols/protocol_block_in_31800.hpp>

#include <algorithm>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chasers.hpp>
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS protocol_block_in_31800

using namespace system;
using namespace network;
using namespace network::messages;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// start/stop
// ----------------------------------------------------------------------------
void protocol_block_in_31800::start() NOEXCEPT
{
    BC_ASSERT(stranded());

    if (started())
        return;

    // Events subscription is asynchronous, events may be missed.
    subscribe_events(BIND(handle_event, _1, _2, _3),
        BIND(handle_complete, _1, _2));

    SUBSCRIBE_CHANNEL(block, handle_receive_block, _1, _2);
    protocol::start();
}

// protected
void protocol_block_in_31800::handle_complete(const code& ec,
    object_key) NOEXCEPT
{
    if (stopped(ec))
        return;

    POST(do_handle_complete, ec);
}

// private
void protocol_block_in_31800::do_handle_complete(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());

    // stopped() is true before stopping() is called (by base).
    if (stopped(ec))
    {
        unsubscribe_events();
        return;
    }

    // Start performance timing and download cycles if candidates are current.
    // This prevents a startup delay in which the node waits on a header.
    if (is_current())
    {
        start_performance();
        get_hashes(BIND(handle_get_hashes, _1, _2));
    }
}

// If this is invoked before do_handle_complete then it will unsubscribe.
void protocol_block_in_31800::stopping(const code& ec) NOEXCEPT
{
    BC_ASSERT(stranded());
    restore(map_);
    map_ = chaser_check::empty_map();
    stop_performance();
    unsubscribe_events();
    protocol::stopping(ec);
}

// handle events (download, split)
// ----------------------------------------------------------------------------

bool protocol_block_in_31800::is_idle() const NOEXCEPT
{
    return map_->empty();
}

bool protocol_block_in_31800::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    // Do not pass ec to stopped as it is not a call status.
    if (stopped())
        return false;

    switch (event_)
    {
        case chase::split:
        {
            // chase::split is posted by notify_one() using subscription key.
            // 'value' is the channel that requested a split to obtain work.
            POST(do_split, channel_t{});
            break;
        }
        case chase::stall:
        {
            // If this channel has divisible work, split it and stop.
            // There are no channels reporting work, either stalled or done.
            // This is initiated by any channel notifying chase::starved.
            if (map_->size() > one)
            {
                POST(do_split, channel_t{});
            }

            break;
        }
        case chase::purge:
        {
            // If have work clear it and stop.
            // This is initiated by chase::regressed/disorganized.
            if (map_->size() > one)
            {
                POST(do_purge, channel_t{});
            }

            break;
        }
        case chase::download:
        {
            // There are count blocks to download at/above given header.
            // chase::header is only sent for current candidate chain, and this
            // chase::download is only sent as a consequence of chase::header.
            POST(do_get_downloads, possible_narrow_cast<size_t>(value));
            break;
        }
        case chase::report:
        {
            POST(do_report, possible_narrow_cast<count_t>(value));
            break;
        }
        case chase::bypass:
        {
            POST(do_bypass, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::stop:
        {
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
}

void protocol_block_in_31800::do_get_downloads(count_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    if (is_idle())
    {
        // Assume performance was stopped due to exhaustion.
        start_performance();
        get_hashes(BIND(handle_get_hashes, _1, _2));
    }
}

void protocol_block_in_31800::do_purge(channel_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!map_->empty())
    {
        LOGV("Purge work (" << map_->size() << ") from [" << authority() << "].");
        map_->clear();
        stop(error::sacrificed_channel);
    }
}

void protocol_block_in_31800::do_split(channel_t) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
        return;

    LOGV("Divide work (" << map_->size() << ") from [" << authority() << "].");
    restore(chaser_check::split(map_));
    restore(map_);
    map_ = chaser_check::empty_map();
    stop(error::sacrificed_channel);
}

void protocol_block_in_31800::do_report(count_t sequence) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Uses application logging since it outputs to a runtime option.
    LOGA("Work report [" << sequence << "] is (" << map_->size() << ") for ["
        << authority() << "].");
}

void protocol_block_in_31800::do_bypass(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    bypass_ = height;
}

// request hashes
// ----------------------------------------------------------------------------

void protocol_block_in_31800::send_get_data(const map_ptr& map) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped())
    {
        restore(map);
        return;
    }

    if (map->empty())
        return;

    if (is_idle())
    {
        const auto message = create_get_data((map_ = map));
        SEND(message, handle_send, _1);
        return;
    }

    // There are two populated maps, return the new and leave the old alone.
    restore(map);
}

get_data protocol_block_in_31800::create_get_data(
    const map_ptr& map) const NOEXCEPT
{
    get_data getter{};
    getter.items.reserve(map->size());

    // bip144: get_data uses witness constant but inventory does not.
    // clang emplace_back bug (no matching constructor), using push_back.
    std::for_each(map->pos_begin(), map->pos_end(),
        [&](const auto& item) NOEXCEPT
        {
            getter.items.push_back({ block_type_, item.hash });
        });

    return getter;
}

// check block
// ----------------------------------------------------------------------------

bool protocol_block_in_31800::handle_receive_block(const code& ec,
    const block::cptr& message) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (stopped(ec))
        return false;

    // Preconditions (requested and not malleated).
    // ........................................................................

    auto& query = archive();
    const chain::block::cptr block_ptr{ message->block_ptr };
    const auto hash = block_ptr->hash();
    const auto it = map_->find(hash);

    if (it == map_->end())
    {
        // Allow unrequested block, not counted toward performance.
        LOGR("Unrequested block [" << encode_hash(hash) << "] from ["
            << authority() << "].");
        return true;
    }

    if (query.is_malleated64(*block_ptr))
    {
        // Disallow known block malleation, drop peer and keep trying.
        // Malleation is assumed (and archived) when malleable64 is invalid.
        LOGR("Malleated64 block [" << encode_hash(hash) << "] from ["
            << authority() << "].");
        stop(error::malleated_block);
        return false;
    }

    const auto& link = it->link;
    const auto& ctx = it->context;

    // Check block.
    // ........................................................................

    // Transaction/witness commitments are required under checkpoint.
    // This ensures that the block/header hash represents expected txs.
    const auto bypass = is_under_bypass(ctx.height) &&
        !block_ptr->is_malleable64();

    // Performs full check if block is mally64 (mally32 caught either way).
    if (const auto code = check(*block_ptr, ctx, bypass))
    {
        // Malleated32 is never associated, so drop peer and continue.
        // Cannot mark unconfirmable as confirmable with same hash may exist.
        // Do not rely on return code because does not catch non-bypass mally.
        if (block_ptr->is_malleated32())
        {
            LOGR("Malleated32 block [" << encode_hash(hash) << ":"
                << ctx.height << "] from [" << authority() << "] "
                << code.message());
            stop(code);
            return false;
        }

        // Malleable64 has not been associated, so drop peer and continue.
        // Cannot mark unconfirmable as confirmable with same hash may exist.
        if (block_ptr->is_malleable64())
        {
            LOGR("Malleable64 block failed check [" << encode_hash(hash) << ":"
                << ctx.height << "] from [" << authority() << "] "
                << code.message());
            stop(code);
            return false;
        }

        // Set invalid non-malleable header to unconfirmable state.
        // Mark unconfirmable as block is neither malleated32 nor malleable64.
        if (!query.set_block_unconfirmable(link))
        {
            LOGF("Failure setting block unconfirmable [" << encode_hash(hash)
                << ":" << ctx.height << "] from [" << authority() << "].");
            fault(error::set_block_unconfirmable);
            return false;
        }

        // Non-malleable block failed block check and was set unconfirmable. 
        LOGR("Block failed check [" << encode_hash(hash) << ":" << ctx.height
            << "] from [" << authority() << "] " << code.message());
        notify(error::success, chase::unchecked, link);
        fire(events::block_unconfirmable, ctx.height);
        stop(code);
        return false;
    }

    // Commit block.txs.
    // ........................................................................

    const auto size = block_ptr->serialized_size(true);
    const chain::transactions_cptr txs_ptr{ block_ptr->transactions_ptr() };

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    // TODO: ensure that when a mally64 is caught under bypass that tx
    // confirmations are reverted when the block is sequentially invalidated.
    // Query: A strong_tx may be in a not-yet-confirmed block.
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    if (const auto code = query.set_code(*txs_ptr, link, size, bypass))
    {
        LOGF("Failure storing block [" << encode_hash(hash) << ":"
            << ctx.height << "] from [" << authority() << "] "
            << code.message());

        stop(fault(code));
        return false;
    }

    // Advance.
    // ........................................................................

    LOGP("Downloaded block [" << encode_hash(hash) << ":" << ctx.height
        << "] from [" << authority() << "].");

    notify(error::success, chase::checked, ctx.height);
    fire(events::block_archived, ctx.height);

    count(message->cached_size);
    map_->erase(it);
    if (is_idle())
        get_hashes(BIND(handle_get_hashes, _1, _2));

    return true;
}

bool protocol_block_in_31800::is_under_bypass(size_t height) const NOEXCEPT
{
    return height <= bypass_;
}

code protocol_block_in_31800::check(const chain::block& block,
    const chain::context& ctx, bool bypass) const NOEXCEPT
{
    code ec{};

    // Transaction commitments and malleated32 are checked under bypass.
    if ((ec = block.check(bypass)))
        return ec;

    // Witnessed tx commitments are checked under bypass (if bip141).
    if ((ec = block.check(ctx, bypass)))
        return ec;

    return system::error::block_success;
}

// get/put hashes
// ----------------------------------------------------------------------------

void protocol_block_in_31800::restore(const map_ptr& map) NOEXCEPT
{
    if (!map->empty())
        put_hashes(map, BIND(handle_put_hashes, _1, map->size()));
}

void protocol_block_in_31800::handle_put_hashes(const code& ec,
    size_t count) NOEXCEPT
{
    LOGV("Put (" << count << ") work for [" << authority() << "].");

    if (ec)
    {
        LOGF("Error putting work for [" << authority() << "] " << ec.message());
    }
}

void protocol_block_in_31800::handle_get_hashes(const code& ec,
    const map_ptr& map) NOEXCEPT
{
    LOGV("Got (" << map->size() << ") work for [" << authority() << "].");

    if (stopped())
    {
        restore(map);
        return;
    }

    if (ec)
    {
        LOGF("Error getting work for [" << authority() << "] " << ec.message());
        stop(ec);
        return;
    }

    if (map->empty())
    {
        notify(error::success, chase::starved, events_key());
        return;
    }

    POST(send_get_data, map);
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
