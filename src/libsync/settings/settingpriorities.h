/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SETTINGPRIORITIES_H
#define SETTINGPRIORITIES_H

namespace OCC::SettingPriority
{

// Highest first; the resolver picks the highest.
constexpr auto userPolicy = 210;
constexpr auto machinePolicy = 200;
constexpr auto serverEnforced = 100;
constexpr auto userConfig = 50;
// Written by earlier versions at the top level of the config file.
constexpr auto legacyUserConfig = 49;
constexpr auto serverDefault = 30;
constexpr auto deviceDefault = 20;

} // namespace OCC::SettingPriority

#endif // SETTINGPRIORITIES_H
