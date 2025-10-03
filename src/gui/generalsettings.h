/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_GENERALSETTINGS_H
#define MIRALL_GENERALSETTINGS_H

#include <QWidget>
#include <QPointer>

namespace OCC {
class IgnoreListEditor;
class SyncLogDialog;

namespace Ui {
    class GeneralSettings;
}

/**
 * @brief The GeneralSettings class
 * @ingroup gui
 */
class GeneralSettings : public QWidget
{
    Q_OBJECT

public:
    explicit GeneralSettings(QWidget *parent = nullptr);
    ~GeneralSettings();
    QSize sizeHint() const override;

public slots:
    void slotStyleChanged();

private slots:
    void saveMiscSettings();
    void slotToggleLaunchOnStartup(bool);
    void slotToggleOptionalServerNotifications(bool);
    void slotShowInExplorerNavigationPane(bool);
    void slotIgnoreFilesEditor();
    void slotCreateDebugArchive();
    void loadMiscSettings();
    void slotShowLegalNotice();
#if defined(BUILD_UPDATER)
    void slotUpdateInfo();
    void slotUpdateChannelChanged(const QString &channel);
    void slotUpdateCheckNow();
    void slotToggleAutoUpdateCheck();
#endif

private:
    void customizeStyle();

    Ui::GeneralSettings *_ui;
    QPointer<IgnoreListEditor> _ignoreEditor;
    bool _currentlyLoading = false;
};


} // namespace OCC
#endif // MIRALL_GENERALSETTINGS_H
