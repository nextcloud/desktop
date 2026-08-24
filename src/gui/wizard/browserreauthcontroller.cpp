/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "browserreauthcontroller.h"

#include <QCoreApplication>

namespace OCC {

BrowserReAuthController::BrowserReAuthController(Account *account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
}

BrowserReAuthController::~BrowserReAuthController() = default;

bool BrowserReAuthController::busy() const
{
    return _busy;
}

bool BrowserReAuthController::authPolling() const
{
    return _authPolling;
}

bool BrowserReAuthController::finished() const
{
    return _finished;
}

QString BrowserReAuthController::errorText() const
{
    return _errorText;
}

QString BrowserReAuthController::infoText() const
{
    return _infoText;
}

QUrl BrowserReAuthController::loginUrl() const
{
    return _loginUrl;
}

void BrowserReAuthController::setErrorText(const QString &errorText)
{
    if (_errorText == errorText) {
        return;
    }

    _errorText = errorText;
    Q_EMIT errorTextChanged();
}

void BrowserReAuthController::setInfoText(const QString &infoText)
{
    if (_infoText == infoText) {
        return;
    }

    _infoText = infoText;
    Q_EMIT infoTextChanged();
}

void BrowserReAuthController::start()
{
    if (_finished || _flow2Auth || !_account) {
        return;
    }

    _flow2Auth = std::make_unique<Flow2Auth>(_account, this);
    connect(_flow2Auth.get(), &Flow2Auth::result, this, &BrowserReAuthController::slotAuthResult, Qt::QueuedConnection);
    connect(_flow2Auth.get(), &Flow2Auth::statusChanged, this, &BrowserReAuthController::slotStatusChanged);
    _flow2Auth->start();
}

void BrowserReAuthController::openBrowserLogin()
{
    if (!_flow2Auth || _finished) {
        return;
    }

    setErrorText({});
    _flow2Auth->openBrowser();
}

void BrowserReAuthController::copyLoginLink()
{
    if (!_flow2Auth || _finished) {
        return;
    }

    setErrorText({});
    _flow2Auth->copyLinkToClipboard();
}

void BrowserReAuthController::pollNow()
{
    if (_flow2Auth && !_finished) {
        _flow2Auth->slotPollNow();
    }
}

void BrowserReAuthController::cancel()
{
    if (_finished) {
        return;
    }

    _flow2Auth.reset();
    setAuthPolling(false);
    setBusy(false);
    setFinished(true);
    Q_EMIT cancelled();
}

void BrowserReAuthController::slotAuthResult(Flow2Auth::Result result, const QString &errorString, const QString &user, const QString &appPassword)
{
    if (_finished) {
        return;
    }

    switch (result) {
    case Flow2Auth::NotSupported:
        setBusy(false);
        setErrorText(QCoreApplication::translate(
            "AccountWizardController",
            "Unable to open the Browser, please copy the link to your Browser."));
        break;
    case Flow2Auth::Error:
        setAuthPolling(false);
        setBusy(false);
        setErrorText(errorString);
        break;
    case Flow2Auth::LoggedIn:
        setAuthPolling(false);
        setBusy(false);
        setFinished(true);
        Q_EMIT credentialsReady(user, appPassword);
        break;
    }
}

void BrowserReAuthController::slotStatusChanged(Flow2Auth::PollStatus status, int secondsLeft)
{
    Q_UNUSED(secondsLeft)

    if (_finished) {
        return;
    }

    if (_flow2Auth) {
        setLoginUrl(_flow2Auth->authorisationLink());
    }

    switch (status) {
    case Flow2Auth::statusPollCountdown:
    case Flow2Auth::statusPollNow:
    case Flow2Auth::statusCopyLinkToClipboard:
        setAuthPolling(true);
        setBusy(false);
        break;
    case Flow2Auth::statusFetchToken:
        setAuthPolling(false);
        setBusy(true);
        break;
    }
}

void BrowserReAuthController::setBusy(bool busy)
{
    if (_busy == busy) {
        return;
    }

    _busy = busy;
    Q_EMIT busyChanged();
}

void BrowserReAuthController::setAuthPolling(bool authPolling)
{
    if (_authPolling == authPolling) {
        return;
    }

    _authPolling = authPolling;
    Q_EMIT authPollingChanged();
}

void BrowserReAuthController::setFinished(bool finished)
{
    if (_finished == finished) {
        return;
    }

    _finished = finished;
    Q_EMIT finishedChanged();
}

void BrowserReAuthController::setLoginUrl(const QUrl &loginUrl)
{
    if (_loginUrl == loginUrl) {
        return;
    }

    _loginUrl = loginUrl;
    Q_EMIT loginUrlChanged();
}

} // namespace OCC
