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
#ifndef LIBBITCOIN_NODE_PROTOCOLS_SESSION_HPP
#define LIBBITCOIN_NODE_PROTOCOLS_SESSION_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {
    
/// Common protocol session context.
/// This is in protocols directory because attach requires it.
class BCN_API session
{
public:
    /// Handle performance, base returns false (implied terminate).
    virtual bool performance(size_t bytes) NOEXCEPT;

    /// Configuration settings for all libraries.
    const configuration& config() const NOEXCEPT;

    /// Thread safe synchronous archival interface.
    full_node::query& archive() const NOEXCEPT;

protected:
    DEFAULT_COPY_MOVE_DESTRUCT(session);

    /// Construct/destruct the session.
    session(full_node& node) NOEXCEPT;

private:
    // This is thread safe (mostly).
    full_node& node_;
};

} // namespace node
} // namespace libbitcoin

#endif
