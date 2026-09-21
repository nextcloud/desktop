/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QObject>
#include <functional>
namespace OCC
{
/** Platform operations used by General settings. Defaults use the live platform services. */
struct GeneralSettingsServices {
    static GeneralSettingsServices production();
    std::function<bool()> systemAutoStart;
    std::function<bool()> autoStart;
    std::function<void(bool)> setAutoStart;
    std::function<bool()> fileProviderEnabled;
    std::function<bool()> fileProviderBusy;
    std::function<void(QObject *, const std::function<void()> &)> observeFileProvider;
};
}
