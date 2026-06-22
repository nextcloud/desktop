/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <string>

class ConfigIni
{
public:
    ConfigIni();

    bool load();

    std::wstring getAppName() const;

private:
    std::wstring _appName;
};
