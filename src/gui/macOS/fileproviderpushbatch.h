/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <utility>

namespace OCC::Mac
{

class FileProviderPushBatch
{
public:
    using FileIds = std::set<std::int64_t>;
    using Delivery = std::shared_ptr<const FileIds>;

    void add(std::int64_t id)
    {
        _queued.insert(id);
    }

    bool ready() const
    {
        return !_inFlight && !_queued.empty();
    }

    Delivery beginDelivery()
    {
        if (!ready()) {
            return {};
        }
        _inFlight = std::make_shared<const FileIds>(std::exchange(_queued, {}));
        return _inFlight;
    }

    bool finishDelivery(const Delivery &delivery, bool acknowledged)
    {
        if (!delivery || _inFlight != delivery) {
            return false;
        }
        if (!acknowledged) {
            _queued.insert(delivery->begin(), delivery->end());
        }
        _inFlight.reset();
        return true;
    }

private:
    FileIds _queued;
    Delivery _inFlight;
};

} // namespace OCC::Mac
