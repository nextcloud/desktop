/*
 * Copyright (C) by Michael Schuster <michael@nextcloud.com>
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

#include "flow2authwidget.h"

#include <QDesktopServices>
#include <QProgressBar>
#include <QLoggingCategory>
#include <QLocale>
#include <QMessageBox>

#include <QMenu>
#include <QClipboard>

#include "common/utility.h"
#include "account.h"
#include "theme.h"
#include "wizard/owncloudwizardcommon.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcFlow2AuthWidget, "gui.wizard.flow2authwidget", QtInfoMsg)


Flow2AuthWidget::Flow2AuthWidget(Account *account, QWidget *parent)
    : QWidget(parent),
      _account(account),
      _ui()
{
    _ui.setupUi(this);

    Theme *theme = Theme::instance();
    _ui.topLabel->hide();
    _ui.bottomLabel->hide();
    QVariant variant = theme->customMedia(Theme::oCSetupTop);
    WizardCommon::setupCustomMedia(variant, _ui.topLabel);
    variant = theme->customMedia(Theme::oCSetupBottom);
    WizardCommon::setupCustomMedia(variant, _ui.bottomLabel);

    WizardCommon::initErrorLabel(_ui.errorLabel);

    connect(_ui.openLinkButton, &QCommandLinkButton::clicked, [this] {
        _ui.errorLabel->hide();
        if (_asyncAuth)
            _asyncAuth->openBrowser();
    });
    _ui.openLinkButton->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(_ui.openLinkButton, &QWidget::customContextMenuRequested, [this](const QPoint &pos) {
        auto menu = new QMenu(_ui.openLinkButton);
        menu->addAction(tr("Copy link to clipboard"), this, [this] {
            if (_asyncAuth)
                QApplication::clipboard()->setText(_asyncAuth->authorisationLink().toString(QUrl::FullyEncoded));
        });
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(_ui.openLinkButton->mapToGlobal(pos));
    });

    _asyncAuth.reset(new Flow2Auth(_account, this));
    connect(_asyncAuth.data(), &Flow2Auth::result, this, &Flow2AuthWidget::asyncAuthResult, Qt::QueuedConnection);
    _asyncAuth->start();
}

void Flow2AuthWidget::asyncAuthResult(Flow2Auth::Result r, const QString &user,
    const QString &appPassword)
{
    switch (r) {
    case Flow2Auth::NotSupported:
        /* Flow2Auth can't open browser */
        _ui.errorLabel->setText(tr("Unable to open the Browser, please copy the link to your Browser."));
        _ui.errorLabel->show();
        break;
    case Flow2Auth::Error:
        /* Error while getting the access token.  (Timeout, or the server did not accept our client credentials */
        _ui.errorLabel->show();
        break;
    case Flow2Auth::LoggedIn: {
        _user = user;
        _appPassword = appPassword;
        emit urlCatched(_user, _appPassword, QString());
        break;
    }
    }
}

void Flow2AuthWidget::setError(const QString &error) {
    if (error.isEmpty()) {
        _ui.errorLabel->hide();
    } else {
        _ui.errorLabel->setText(error);
        _ui.errorLabel->show();
    }
}

Flow2AuthWidget::~Flow2AuthWidget() {
    _asyncAuth.reset();

    // Forget sensitive data
    _appPassword.clear();
    _user.clear();
}

}
