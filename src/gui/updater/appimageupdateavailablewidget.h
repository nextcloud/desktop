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

#include <QDialog>
#include <QScopedPointer>
#include <QVersionNumber>

namespace Ui {
class AppImageUpdateAvailableWidgetUi;
}

namespace OCC {

/**
 * @brief Dialog shown when updates for the running AppImage are available
 * @ingroup gui
 */
class AppImageUpdateAvailableWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AppImageUpdateAvailableWidget(const QVersionNumber &currentVersion, const QVersionNumber &newVersion, QWidget *parent = nullptr);

    ~AppImageUpdateAvailableWidget() override;

Q_SIGNALS:
    /**
     * Emitted when an update is explicitly skipped by the user.
     */
    void skipUpdateButtonClicked();

    /// Emitted when the cancel button is clicked.
    void rejected();

    /// Emitted when the ok button is clicked.
    void accepted();

private:
    ::Ui::AppImageUpdateAvailableWidgetUi *_ui;
};

}
