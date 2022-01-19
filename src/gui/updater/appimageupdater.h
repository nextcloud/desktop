/*
* Copyright (C) 2022 by Fabian Müller <fmueller@owncloud.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*/

#pragma once

#include <QObject>
#include <QString>

#include <updater/ocupdater.h>

namespace OCC {

/**
 * @brief AppImage Updater using AppImageUpdate
 * @ingroup gui
 */
class AppImageUpdater : public OCUpdater
{
    Q_OBJECT

public:
    explicit AppImageUpdater(const QUrl &url);
    bool handleStartup() override;
    void backgroundCheckForUpdate() override;

private:
    void versionInfoArrived(const UpdateInfo &succeeded) override;
};

} // namespace OCC
