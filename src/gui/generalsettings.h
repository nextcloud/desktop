/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_GENERALSETTINGS_H
#define MIRALL_GENERALSETTINGS_H

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
    void loadMiscSettings();

private:
    void customizeStyle();

    Ui::GeneralSettings *_ui;
    bool _currentlyLoading = false;
};

} // namespace OCC

#endif // MIRALL_GENERALSETTINGS_H
