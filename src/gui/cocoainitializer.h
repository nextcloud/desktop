/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

namespace OCC {
namespace Mac {

/**
 * @brief CocoaInitializer provides an AutoRelease Pool via RIIA for use in main()
 * @ingroup gui
 */
class CocoaInitializer {
public:
    CocoaInitializer();
    ~CocoaInitializer();
private:
    class Private;
    Private *d;
};

} // namespace Mac
} // namespace OCC
