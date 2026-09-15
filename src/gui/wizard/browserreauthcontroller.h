/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BROWSERREAUTHCONTROLLER_H
#define BROWSERREAUTHCONTROLLER_H

#include "accountfwd.h"
#include "creds/flow2auth.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class TestBrowserReAuthController;

namespace OCC {

/**
 * @brief Controls browser-based re-authentication for an existing account.
 *
 * The controller only returns replacement credentials. It never creates or
 * registers an account.
 */
class BrowserReAuthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool authPolling READ authPolling NOTIFY authPollingChanged)
    Q_PROPERTY(bool finished READ finished NOTIFY finishedChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString infoText READ infoText NOTIFY infoTextChanged)
    Q_PROPERTY(QUrl loginUrl READ loginUrl NOTIFY loginUrlChanged)

public:
    /** @brief Creates a controller that authenticates the existing @p account. */
    explicit BrowserReAuthController(Account *account, QObject *parent = nullptr);
    ~BrowserReAuthController() override;

    /** @brief Returns whether an authentication request is currently being processed. */
    [[nodiscard]] bool busy() const;
    /** @brief Returns whether the controller is polling for browser authorization. */
    [[nodiscard]] bool authPolling() const;
    /** @brief Returns whether authentication was completed or cancelled. */
    [[nodiscard]] bool finished() const;
    /** @brief Returns the authentication error shown to the user. */
    [[nodiscard]] QString errorText() const;
    /** @brief Returns the existing-account information shown to the user. */
    [[nodiscard]] QString infoText() const;
    /** @brief Returns the browser authorization URL when it is available. */
    [[nodiscard]] QUrl loginUrl() const;

    /** @brief Replaces the existing-account information shown to the user. */
    void setInfoText(const QString &infoText);

    /** @brief Starts browser authentication once. */
    void start();
    /** @brief Opens the authorization URL in the default browser. */
    Q_INVOKABLE void openBrowserLogin();
    /** @brief Copies the authorization URL to the clipboard. */
    Q_INVOKABLE void copyLoginLink();
    /** @brief Requests an immediate authorization poll. */
    Q_INVOKABLE void pollNow();
    /** @brief Cancels authentication and finishes the controller. */
    Q_INVOKABLE void cancel();

private Q_SLOTS:
    /** @brief Handles the final result produced by the browser authentication flow. */
    void slotAuthResult(OCC::Flow2Auth::Result result, const QString &errorString, const QString &user, const QString &appPassword);
    /** @brief Updates the presentation state when browser authentication changes phase. */
    void slotStatusChanged(OCC::Flow2Auth::PollStatus status, int secondsLeft);

Q_SIGNALS:
    /** @brief Emitted when busy changes. */
    void busyChanged();
    /** @brief Emitted when authPolling changes. */
    void authPollingChanged();
    /** @brief Emitted when finished changes. */
    void finishedChanged();
    /** @brief Emitted when errorText changes. */
    void errorTextChanged();
    /** @brief Emitted when infoText changes. */
    void infoTextChanged();
    /** @brief Emitted when loginUrl changes. */
    void loginUrlChanged();
    /** @brief Provides replacement credentials after successful authentication. */
    void credentialsReady(const QString &user, const QString &appPassword);
    /** @brief Reports that authentication was cancelled. */
    void cancelled();

private:
    friend class ::TestBrowserReAuthController;

    void setErrorText(const QString &errorText);
    void setBusy(bool busy);
    void setAuthPolling(bool authPolling);
    void setFinished(bool finished);
    void setLoginUrl(const QUrl &loginUrl);

    Account *_account = nullptr;
    std::unique_ptr<Flow2Auth> _flow2Auth;
    bool _busy = false;
    bool _authPolling = false;
    bool _finished = false;
    QString _errorText;
    QString _infoText;
    QUrl _loginUrl;
};

} // namespace OCC

#endif // BROWSERREAUTHCONTROLLER_H
