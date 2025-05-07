/*
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

namespace OCC {
namespace Mac {

    /**
 * @brief CocoaInitializer provides an AutoRelease Pool via RIIA for use in main()
 * @ingroup gui
 */
    class CocoaInitializer
    {
    public:
        CocoaInitializer();
        ~CocoaInitializer();

    private:
        class Private;
        Private *d;
    };

} // namespace Mac
} // namespace OCC
