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
#include <bitcoin/node/chasers/chaser_check.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <variant>
#include <bitcoin/network.hpp>
#include <bitcoin/node/error.hpp>
#include <bitcoin/node/full_node.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_check

using namespace network;
using namespace system;
using namespace system::chain;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)

chaser_check::chaser_check(full_node& node) NOEXCEPT
  : chaser(node),
    connections_(node.network_settings().outbound_connections),
    inventory_(system::lesser(node.node_settings().maximum_inventory,
        network::messages::max_inventory))
{
}

chaser_check::~chaser_check() NOEXCEPT
{
}

// utility
// ----------------------------------------------------------------------------
// private

size_t chaser_check::count_map(const maps& table) const NOEXCEPT
{
    return std::accumulate(table.begin(), table.end(), zero,
        [](size_t sum, const map_ptr& map) NOEXCEPT
        {
            return sum + map->size();
        });
}

void chaser_check::initialize_map(maps& table) const NOEXCEPT
{
    auto start = archive().get_fork();
    while (true)
    {
        const auto map = make_map(start, inventory_);
        if (map->empty()) break;
        table.push_front(map);
        start = map->top().height;
    }
}

chaser_check::map_ptr chaser_check::make_map(size_t start,
    size_t count) const NOEXCEPT
{
    return std::make_shared<database::associations>(
        archive().get_unassociated_above(start, count));
}

chaser_check::map_ptr chaser_check::get_map(maps& table) NOEXCEPT
{
    return table.empty() ? std::make_shared<database::associations>() :
        pop(table);
}

// start
// ----------------------------------------------------------------------------

code chaser_check::start() NOEXCEPT
{
    BC_ASSERT(node_stranded());

    initialize_map(map_table_);
    return SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
}

// event handlers
// ----------------------------------------------------------------------------

void chaser_check::handle_event(const code&, chase event_,
    link value) NOEXCEPT
{
    if (event_ == chase::header)
    {
        BC_ASSERT(std::holds_alternative<chaser::height_t>(value));
        POST(handle_header, std::get<height_t>(value));
    }
}

// Stale branches are just be allowed to complete (still downloaded).
void chaser_check::handle_header(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());

    // This can produce duplicate downloads in relation to those outstanding,
    // which is ok. That implies a rerg and then a reorg back before complete.
    do_put_hashes(make_map(branch_point), BIND(handle_put_hashes, _1));
}

void chaser_check::handle_put_hashes(const code&) NOEXCEPT
{
    BC_ASSERT(stranded());
}

// methods
// ----------------------------------------------------------------------------

void chaser_check::get_hashes(handler&& handler) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_check::do_get_hashes,
            this, std::move(handler)));
}

void chaser_check::put_hashes(const map_ptr& map,
    network::result_handler&& handler) NOEXCEPT
{
    boost::asio::post(strand(),
        std::bind(&chaser_check::do_put_hashes,
            this, map, std::move(handler)));
}

void chaser_check::do_get_hashes(const handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    const auto map = get_map(map_table_);

    ////LOGN("Hashes -" << map->size() << " ("
    ////    << count_map(map_table_) << ") remain.");

    handler(error::success, map);
}

void chaser_check::do_put_hashes(const map_ptr& map,
    const network::result_handler& handler) NOEXCEPT
{
    BC_ASSERT(stranded());

    if (!map->empty())
    {
        map_table_.push_back(map);
        notify(error::success, chase::download,
            system::possible_narrow_cast<count_t>(map->size()));
    }

    ////LOGN("Hashes +" << map->size() << " ("
    ////    << count_map(map_table_) << ") remain.");

    handler(error::success);
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin
