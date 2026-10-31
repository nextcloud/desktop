/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LOWPOWERGPU_MAC_H
#define LOWPOWERGPU_MAC_H

class QCoreApplication;

namespace OCC {
namespace Mac {

/**
 * @brief Render Qt Quick windows on the integrated GPU on Macs with automatic graphics switching.
 *
 * Qt's Metal backend always uses MTLCreateSystemDefaultDevice(), which is the discrete GPU on
 * MacBook Pros that have two. Holding that device keeps the whole machine on the discrete GPU
 * until the client quits, which drains the battery. This hands every Qt Quick window the
 * low-power device instead, before its scene graph starts.
 *
 * Does nothing on Macs with a single GPU (including all Apple silicon Macs).
 * Must be called before the first Qt Quick window is created.
 *
 * @param app  the application; the event filter is installed on it and parented to it.
 * @ingroup gui
 */
void preferLowPowerGpu(QCoreApplication *app);

} // namespace Mac
} // namespace OCC

#endif
