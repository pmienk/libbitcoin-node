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
#include <bitcoin/node/chasers/chaser_validate.hpp>

#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace system::chain;
using namespace system::neutrino;
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_validate::chaser_validate(full_node& node) NOEXCEPT
  : chaser(node),
    initial_subsidy_(node.config().bitcoin.initial_subsidy()),
    subsidy_interval_blocks_(node.config().bitcoin.subsidy_interval_blocks)
{
}

code chaser_validate::start() NOEXCEPT
{
    update_position(archive().get_fork());
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

bool chaser_validate::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating message/query traffic from the candidate chain.
    if (suspended())
        return true;

    // These come out of order, advance in order asynchronously.
    // Asynchronous completion results in out of order notification.
    switch (event_)
    {
        // Track downloaded.
        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case chase::start:
        case chase::bump:
        {
            POST(do_bump, height_t{});
            break;
        }
        case chase::checked:
        {
            POST(do_checked, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::regressed:
        {
            POST(do_regressed, possible_narrow_cast<height_t>(value));
            break;
        }
        case chase::disorganized:
        {
            POST(do_disorganized, possible_narrow_cast<height_t>(value));
            break;
        }
        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

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

// track downloaded in order (to validate)
// ----------------------------------------------------------------------------

void chaser_validate::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    // If branch point is at or above last validated there is nothing to do.
    if (branch_point < position())
        do_disorganized(branch_point);
}

void chaser_validate::do_disorganized(height_t top) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Revert to confirmed top as the candidate chain is fully reverted.
    update_position(top);
    do_checked(top);
}

void chaser_validate::do_checked(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Candidate block was checked at the given height, validate/advance.
    if (height == add1(position()))
        do_bump(height);
}

void chaser_validate::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // Validate checked blocks starting immediately after last validated.
    for (auto height = add1(position()); !closed(); ++height)
    {
        // Precondition (associated).
        // ....................................................................

        const auto link = query.to_candidate(height);
        if (!query.is_associated(link))
            return;

        // Accept/Connect block.
        // ....................................................................

        // neutrino_ is advanced here if successful or bypassed.
        if (const auto code = validate(link, height))
        {
            if (code == error::validation_bypass ||
                code == database::error::block_confirmable ||
                code == database::error::block_valid)
            {
                // Advance.
                ++position();
                notify(code, chase::valid, height);
                fire(events::validate_bypassed, height);
                continue;
            }

            if (code == error::store_integrity)
            {
                fault(error::node_validate);
                return;
            }

            if (query.is_malleable(link))
            {
                notify(code, chase::malleated, link);
                fire(events::block_malleated, height);
            }
            else
            {
                if (code != database::error::block_unconfirmable &&
                    !query.set_block_unconfirmable(link))
                {
                    fault(error::set_block_unconfirmable);
                    return;
                }

                notify(code, chase::unvalid, link);
                fire(events::block_unconfirmable, height);
            }

            LOGR("Unvalidated block [" << height << "] " << code.message());
            return;
        }

        // Commit validation metadata.
        // ....................................................................

        // TODO: set fees on valid because prevout.value() populated.
        // TODO: in concurrent model sum fees from each validated_tx record.
        // [set_txs_connected] FOR PERFORMANCE EVALUATION ONLY.
        // Tx validation/states are independent of block validation.
        if (!query.set_txs_connected(link))
        {
            fault(error::set_txs_connected);
            return;
        }

        if (!query.set_block_valid(link))
        {
            fault(error::set_block_valid);
            return;
        }

        // Advance.
        // ....................................................................

        ++position();
        notify(error::success, chase::valid, height);
        fire(events::block_validated, height);
    }
}

code chaser_validate::validate(const header_link& link,
    size_t height) NOEXCEPT
{
    code ec{};
    const auto& query = archive();
    if (is_under_bypass(height) && !query.is_malleable(link))
        return update_neutrino(link) ? error::validation_bypass :
            error::store_integrity;

    ec = query.get_block_state(link);
    if (ec == database::error::block_confirmable ||
        ec == database::error::block_unconfirmable ||
        ec == database::error::block_valid)
        return ec;

    database::context context{};
    const auto block_ptr = query.get_block(link);
    if (!block_ptr || !query.get_context(context, link))
        return error::store_integrity;

    const auto& block = *block_ptr;
    if (!query.populate(block))
        return system::error::missing_previous_output;

    const chain::context ctx
    {
        context.flags,  // [accept & connect]
        {},             // timestamp
        {},             // mtp
        context.height, // [accept]
        {},             // minimum_block_version
        {}              // work_required
    };

    if ((ec = block.accept(ctx, subsidy_interval_blocks_, initial_subsidy_)))
        return ec;

    if ((ec = block.connect(ctx)))
        return ec;

    // This can only fail if block missing or prevouts are not fully populated.
    return update_neutrino(link, block) ? ec : error::store_integrity;
}

// neutrino
// ----------------------------------------------------------------------------

// Returns null_hash if not found, intended for genesis block.
hash_digest chaser_validate::get_neutrino(size_t height) const NOEXCEPT
{
    hash_digest neutrino{};
    const auto& query = archive();
    if (query.neutrino_enabled())
        query.get_filter_head(neutrino, query.to_candidate(height));

    return neutrino;
}

// This can only fail if block missing or prevouts are not fully populated.
bool chaser_validate::update_neutrino(const header_link& link) NOEXCEPT
{
    const auto& query = archive();
    if (!query.neutrino_enabled())
        return true;

    const auto block_ptr = query.get_block(link);
    if (!block_ptr)
        return false;

    const auto& block = *block_ptr;
    return query.populate(block) && update_neutrino(link, block);
}

// This can only fail if prevouts are not fully populated.
bool chaser_validate::update_neutrino(const header_link& link,
    const chain::block& block) NOEXCEPT
{
    auto& query = archive();
    if (!query.neutrino_enabled())
        return true;

    data_chunk filter{};
    if (!compute_filter(filter, block))
        return false;

    neutrino_ = compute_filter_header(neutrino_, filter);
    return query.set_filter(link, neutrino_, filter);
}


void chaser_validate::update_position(size_t height) NOEXCEPT
{
    set_position(height);
    neutrino_ = get_neutrino(position());
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
