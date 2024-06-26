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
#ifndef LIBBITCOIN_NODE_CHASE_HPP
#define LIBBITCOIN_NODE_CHASE_HPP

#include <bitcoin/node/events.hpp>

namespace libbitcoin {
namespace node {

enum class chase
{
    /// Work shuffling.
    /// -----------------------------------------------------------------------

    /// Chasers directed to start operating (height_t).
    /// Issued by 'full_node' and handled by 'validate'.
    start,

    /// Disk space is limited (count_t).
    /// Issued by full_node and handled by 'snapshot' and 'storage'.
    space,

    /// Chaser is directed to start when there are no downloads (height_t).
    /// Issued by 'organize' and handled by 'validate'.
    bump,

    /// Channels (all) directed to stop (channel_t).
    /// Issued by 'full_node' and handled by 'observer'.
    suspend,

    /// Channel starved for work (object_t).
    /// Issued by 'block_in_31800' and handled by 'session_outbound'.
    starved,

    /// Channel (slow) directed to split work and stop (object_t).
    /// Issued by 'session_outbound' and handled by 'block_in_31800'.
    split,

    /// Channels (all with work) directed to split work and stop (channel_t).
    /// Issued by 'session_outbound' and handled by 'block_in_31800'.
    stall,

    /// Channels (all with work) directed to drop work and stop (channel_t).
    /// Issued by 'check' and handled by 'block_in_31800'.
    purge,

    /// Channels (all) directed to write work count to the log (count_t).
    /// Issued by 'executor' and handled by 'block_in_31800'.
    report,

    /// Candidate Chain.
    /// -----------------------------------------------------------------------

    /// Legacy: A new strong branch exists (height_t).
    /// Issued by 'block' and handled by 'confirm'.
    block,

    /// A new candidate branch exists (height_t).
    /// Issued by 'header' and handled by 'check'.
    header,

    /// New candidate headers without txs exist (count_t).
    /// Issued by 'check' and handled by 'block_in_31800'.
    download,

    /// The candidate chain has been reorganized (branched below its top).
    /// Issued by 'organize' and handled by 'check'.
    regressed,

    /// Late-stage Invalidity.
    /// -----------------------------------------------------------------------

    /// unchecked, unvalid or unconfirmable was handled (height_t).
    /// Issued by 'organize' and handled by 'validate' (disorgs candidates).
    disorganized,

    /// stored block was determined to be malleated (invalid) (header_t).
    /// Issued by 'validate' and 'confirm', handled by 'chaser' (redownload).
    malleated,

    /// Validation.
    /// -----------------------------------------------------------------------

    /// A block has been downloaded, checked and stored (height_t).
    /// Issued by 'block_in_31800' and handled by 'connect'.
    checked,

    /// A downloaded block has failed check (header_t).
    /// Issued by 'block_in_31800' and handled by 'header'.
    unchecked,

    /// A branch has become valid (height_t).
    /// Issued by 'validate' and handled by 'confirm'.
    valid,

    /// A checked block has failed validation (header_t).
    /// Issued by 'validate' and handled by 'header'.
    unvalid,

    /// Confirmation.
    /// -----------------------------------------------------------------------

    /// A connected block has become confirmable (header_t).
    /// Issued by 'confirm' and handled by 'transaction'.
    confirmable,

    /// A connected block has failed confirmability (header_t).
    /// Issued by 'confirm' and handled by 'header' (and 'block').
    unconfirmable,

    /// Confirmed Chain.
    /// -----------------------------------------------------------------------

    /// A confirmable block has been confirmed (header_t).
    /// Issued by 'confirm' and handled by 'transaction'.
    organized,

    /// A previously confirmed block has been unconfirmed (header_t).
    /// Issued by 'confirm' and handled by 'transaction'.
    reorganized,

    /// Mining.
    /// -----------------------------------------------------------------------

    /// A new transaction has been added to the pool (transaction_t).
    /// Issued by 'transaction' and handled by 'template'.
    transaction,

    /// A new candidate block (template) has been created (height_t).
    /// Issued by 'template' and handled by [miners].
    template_,

    /// Stop.
    /// -----------------------------------------------------------------------

    /// Service is stopping, accompanied by error::service_stopped (count_t).
    stop
};

} // namespace node
} // namespace libbitcoin

#endif
