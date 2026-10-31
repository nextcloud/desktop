/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <functional>

namespace OCC {
namespace Mac {

/**
 * @brief Keeps native URL handling and the background activity alive while the application runs.
 * @ingroup gui
 */
class CocoaInitializer
{
public:
    /**
     * @brief Run the application on the main thread and drain its autorelease pool after cleanup.
     *
     * Exceptions are rethrown after native resources and the pool have been released.
     */
    static int run(const std::function<int()> &mainFunction);

    // Native hooks and activity only; run() also owns the autorelease pool.
    CocoaInitializer();
    ~CocoaInitializer();

private:
    class Private;
    Private *d;
};

} // namespace Mac
} // namespace OCC
