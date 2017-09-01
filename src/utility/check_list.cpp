/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/utility/check_list.hpp>

#include <cstddef>
#include <list>
#include <utility>
#include <bitcoin/bitcoin.hpp>

namespace libbitcoin {
namespace node {

bool check_list::empty() const
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    shared_lock lock(mutex_);

    return checks_.empty();
    ///////////////////////////////////////////////////////////////////////////
}

size_t check_list::size() const
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    shared_lock lock(mutex_);

    return checks_.size();
    ///////////////////////////////////////////////////////////////////////////
}

void check_list::pop(const hash_digest& hash, size_t height)
{
    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (checks_.empty() || checks_.front().hash() != hash)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return;
    }

    if (checks_.front().height() != height)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        BITCOIN_ASSERT_MSG(false, "popped invalid height for hash");
        return;
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    checks_.pop_back();
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

void check_list::push(hash_digest&& hash, size_t height)
{
    BITCOIN_ASSERT_MSG(height != 0, "pushed genesis height for download");

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (!checks_.empty() && checks_.front().height() >= height)
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        BITCOIN_ASSERT_MSG(false, "pushed height out of order");
        return;
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    checks_.emplace_back(std::move(hash), height);
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

void check_list::enqueue(hash_digest&& hash, size_t height)
{
    BITCOIN_ASSERT_MSG(height != 0, "enqueued genesis height for download");

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (!checks_.empty() && height >= checks_.front().height())
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        BITCOIN_ASSERT_MSG(false, "enqueued height out of order");
        return;
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    checks_.emplace_front(std::move(hash), height);
    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////
}

check_list::checks check_list::extract(size_t divisor, size_t limit)
{
    if (divisor == 0 || limit == 0)
        return{};

    ///////////////////////////////////////////////////////////////////////////
    // Critical Section
    mutex_.lock_upgrade();

    if (checks_.empty())
    {
        mutex_.unlock_upgrade();
        //---------------------------------------------------------------------
        return{};
    }

    mutex_.unlock_upgrade_and_lock();
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    checks result;
    const auto step = divisor - 1u;

    for (auto it = checks_.begin();
        it != checks_.end() && result.size() < limit;
        std::advance(it, step))
    {
        result.push_front(*it);
        it = checks_.erase(it);
    }

    mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    return result;
}

} // namespace node
} // namespace libbitcoin
