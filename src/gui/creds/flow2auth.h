/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QPointer>
#include <QUrl>
#include <QTimer>
#include "accountfwd.h"

class QNetworkReply;

namespace OCC {

/**
 * Job that does the authorization, grants and fetches the access token via Login Flow v2
 *
 * See: https://docs.nextcloud.com/server/latest/developer_manual/client_apis/LoginFlow/index.html#login-flow-v2
 */
class Flow2Auth : public QObject
{
    Q_OBJECT
public:
    enum TokenAction {
        actionOpenBrowser = 1,
        actionCopyLinkToClipboard
    };
    enum PollStatus {
        statusPollCountdown = 1,
        statusPollNow,
        statusFetchToken,
        statusCopyLinkToClipboard
    };

    Flow2Auth(Account *account, QObject *parent);
    ~Flow2Auth() override;

    enum Result { NotSupported,
        LoggedIn,
        Error };
    Q_ENUM(Result);
    void start();
    void openBrowser();
    void copyLinkToClipboard();
    [[nodiscard]] QUrl authorisationLink() const;

signals:
    /**
     * The state has changed.
     * when logged in, appPassword has the value of the app password.
     */
    void result(OCC::Flow2Auth::Result result, const QString &errorString = QString(),
                const QString &user = QString(), const QString &appPassword = QString());

    void statusChanged(const OCC::Flow2Auth::PollStatus status, int secondsLeft);

public slots:
    void slotPollNow();

private slots:
    void slotPollTimerTimeout();

private:
    void fetchNewToken(const TokenAction action);
    [[nodiscard]] QJsonObject handleResponse(QNetworkReply *reply);

    Account *_account;
    QUrl _loginUrl;
    QString _pollToken;
    QString _pollEndpoint;
    QTimer _pollTimer;
    qint64 _secondsLeft = 0LL;
    qint64 _secondsInterval = 0LL;
    bool _isBusy = false;
    bool _hasToken = false;
    bool _enforceHttps = false;
};

} // namespace OCC
