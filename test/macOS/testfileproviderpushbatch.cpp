/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../../src/gui/macOS/fileproviderpushbatch.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

using OCC::Mac::FileProviderPushBatch;

namespace
{
void require(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    FileProviderPushBatch batch;
    batch.add(17);
    batch.add(4);
    batch.add(17);
    const auto initial = batch.beginDelivery();
    require(initial && *initial == FileProviderPushBatch::FileIds{4, 17}, "Deduplicate pending IDs");
    require(!batch.beginDelivery(), "Only one delivery can be in flight per domain");
    batch.add(42);
    require(batch.finishDelivery(initial, false), "A timeout completes the first attempt");
    const auto retry = batch.beginDelivery();
    require(retry && *retry == FileProviderPushBatch::FileIds{4, 17, 42}, "Replay unacknowledged IDs with newly queued changes");
    require(!batch.finishDelivery(initial, true), "A stale reply must not acknowledge a later delivery");
    batch.add(44);
    require(batch.finishDelivery(retry, true), "An acknowledgement completes the active batch");
    const auto next = batch.beginDelivery();
    require(next && *next == FileProviderPushBatch::FileIds{44}, "Retain only changes received after the acknowledged delivery");
    require(batch.finishDelivery(next, true), "The new batch is delivered");
    require(!batch.beginDelivery(), "Acknowledged IDs must not be sent again");
}
