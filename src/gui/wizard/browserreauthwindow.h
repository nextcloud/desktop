/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BROWSERREAUTHWINDOW_H
#define BROWSERREAUTHWINDOW_H

#include "accountfwd.h"

#include <QObject>
#include <QPointer>
#include <QString>

class QQuickWindow;

namespace OCC {

class BrowserReAuthController;

/**
 * @brief Hosts the QML browser re-authentication window.
 *
 * The window presents BrowserReAuthPage and returns replacement credentials
 * for the existing account through BrowserReAuthController.
 */
class BrowserReAuthWindow : public QObject
{
    Q_OBJECT
public:
    /** @brief Creates a re-authentication window for the existing @p account. */
    explicit BrowserReAuthWindow(Account *account, QObject *parent = nullptr);
    ~BrowserReAuthWindow() override;

    /** @brief Sets the account information displayed in the window. */
    void setInfoText(const QString &infoText);
    /** @brief Shows and activates the window, then starts authentication. */
    void show();
    /** @brief Closes and schedules deletion of the window host. */
    void close();

Q_SIGNALS:
    /** @brief Returns replacement credentials after successful authentication. */
    void credentialsReady(const QString &user, const QString &appPassword);
    /** @brief Reports that authentication was cancelled or unavailable. */
    void cancelled();

private Q_SLOTS:
    /** @brief Raises the window after the Settings window is shown. */
    void slotShowSettingsDialog();

private:
    BrowserReAuthController *_controller = nullptr;
    QPointer<QQuickWindow> _window;
    bool _loadFailed = false;
};

} // namespace OCC

#endif // BROWSERREAUTHWINDOW_H
