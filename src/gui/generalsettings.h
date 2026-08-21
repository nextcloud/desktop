/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_GENERALSETTINGS_H
#define MIRALL_GENERALSETTINGS_H

// For BUILD_FILE_PROVIDER_MODULE, which gates the File Provider members below.
#include "config.h"

#include <QWidget>

namespace OCC {

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

private slots:
    void saveMiscSettings();
    void slotToggleLaunchOnStartup(bool);
    void slotToggleOptionalServerNotifications(bool);
    void slotToggleChatNotifications(bool);
    void slotToggleCallNotifications(bool);
    void slotToggleQuotaWarningNotifications(bool);
    void slotFileProviderSwitchClicked(bool checked);
    void loadMiscSettings();

private:
    void customizeStyle();

#ifdef BUILD_FILE_PROVIDER_MODULE
    void confirmEnableFileProviderMode();
    void confirmDisableFileProviderMode();
#endif

    Ui::GeneralSettings *_ui;
    bool _currentlyLoading = false;
};

} // namespace OCC

#endif // MIRALL_GENERALSETTINGS_H
