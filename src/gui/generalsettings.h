/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_GENERALSETTINGS_H
#define MIRALL_GENERALSETTINGS_H

#include "config.h"

#include <QWidget>
#include <QPointer>

namespace OCC {
class IgnoreListEditor;
class SyncLogDialog;
class AccountState;

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
    ~GeneralSettings() override;
    [[nodiscard]] QSize sizeHint() const override;

public slots:
    void slotStyleChanged();
#if defined(BUILD_UPDATER)
    void loadUpdateChannelsList();
    [[nodiscard]] QString updateChannelToLocalized(const QString &channel) const;
    void setAndCheckNewUpdateChannel(const QString &newChannel);
    void restoreUpdateChannel();
#endif

private slots:
    void saveMiscSettings();
    void slotToggleLaunchOnStartup(bool);
    void slotToggleOptionalServerNotifications(bool);
    void slotToggleChatNotifications(bool);
    void slotToggleCallNotifications(bool);
    void slotToggleQuotaWarningNotifications(bool);
    void slotShowInExplorerNavigationPane(bool);
    void slotIgnoreFilesEditor();
    void slotCreateDebugArchive();
    void loadMiscSettings();
    void slotShowLegalNotice();
    void slotRemotePollIntervalChanged(int seconds);
    void updatePollIntervalVisibility();
#if defined(BUILD_UPDATER)
    void slotUpdateInfo();
    void slotUpdateChannelChanged();
    void slotUpdateCheckNow();
    void slotToggleAutoUpdateCheck();
    void slotRestoreUpdateChannel();
#endif

private:
    void customizeStyle();

    Ui::GeneralSettings *_ui;
    QPointer<IgnoreListEditor> _ignoreEditor;
    bool _currentlyLoading = false;
    QStringList _currentUpdateChannelList;
};


} // namespace OCC
#endif // MIRALL_GENERALSETTINGS_H
