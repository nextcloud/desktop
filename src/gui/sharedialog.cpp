/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#include "sharedialog.h"
#include "sharee.h"
#include "sharelinkwidget.h"
#include "shareusergroupwidget.h"
#include "ui_sharedialog.h"

#include "account.h"
#include "accountstate.h"
#include "configfile.h"
#include "theme.h"
#include "thumbnailjob.h"

#include <QFileInfo>
#include <QFileIconProvider>
#include <QPointer>
#include <QPushButton>
#include <QFrame>

namespace OCC {

static const int thumbnailSize = 40;

ShareDialog::ShareDialog(QPointer<AccountState> accountState,
    const QString &sharePath,
    const QString &localPath,
    SharePermissions maxSharingPermissions,
    const QByteArray &numericFileId,
    ShareDialogStartPage startPage,
    QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::ShareDialog)
    , _accountState(accountState)
    , _sharePath(sharePath)
    , _localPath(localPath)
    , _maxSharingPermissions(maxSharingPermissions)
    , _privateLinkUrl(accountState->account()->deprecatedPrivateLinkUrl(numericFileId).toString(QUrl::FullyEncoded))
    , _startPage(startPage)
    , _linkWidget(nullptr)
    , _userGroupWidget(nullptr)
    , _progressIndicator(nullptr)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("SharingDialog"); // required as group for saveGeometry call

    _ui->setupUi(this);

    // We want to act on account state changes
    connect(_accountState.data(), &AccountState::stateChanged, this, &ShareDialog::slotAccountStateChanged);

    // Set icon
    QFileInfo f_info(_localPath);
    QFileIconProvider icon_provider;
    QIcon icon = icon_provider.icon(f_info);
    auto pixmap = icon.pixmap(thumbnailSize, thumbnailSize);
    if (pixmap.width() > 0) {
        _ui->label_icon->setPixmap(pixmap);
    }

    // Set filename
    QFileInfo lPath(_localPath);
    QString fileName = lPath.fileName();
    _ui->label_name->setText(tr("%1").arg(fileName));
    QFont f(_ui->label_name->font());
    f.setPointSize(f.pointSize() * 1.4);
    _ui->label_name->setFont(f);

    QString ocDir(_sharePath);
    ocDir.truncate(ocDir.length() - fileName.length());

    ocDir.replace(QRegExp("^/*"), "");
    ocDir.replace(QRegExp("/*$"), "");

    // Laying this out is complex because sharePath
    // may be in use or not.
    _ui->gridLayout->removeWidget(_ui->label_sharePath);
    _ui->gridLayout->removeWidget(_ui->label_name);
    if (ocDir.isEmpty()) {
        _ui->gridLayout->addWidget(_ui->label_name, 0, 1, 2, 1);
        _ui->label_sharePath->setText(QString());
    } else {
        _ui->gridLayout->addWidget(_ui->label_name, 0, 1, 1, 1);
        _ui->gridLayout->addWidget(_ui->label_sharePath, 1, 1, 1, 1);
        _ui->label_sharePath->setText(tr("Folder: %2").arg(ocDir));
    }

    this->setWindowTitle(tr("%1 Sharing").arg(Theme::instance()->appNameGUI()));

    if (!accountState->account()->capabilities().shareAPI()) {
        //auto label = new QLabel(tr("The server does not allow sharing"));
        //label->setWordWrap(true);
        //label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        //layout()->replaceWidget(_ui->shareWidgets, label);
        return;
    }

    if (QFileInfo(_localPath).isFile()) {
        ThumbnailJob *job = new ThumbnailJob(_sharePath, _accountState->account(), this);
        connect(job, &ThumbnailJob::jobFinished, this, &ShareDialog::slotThumbnailFetched);
        job->start();
    }

//    _progressIndicator = new QProgressIndicator(this);
//    _progressIndicator->startAnimation();
//    _progressIndicator->setToolTip(tr("Retrieving maximum possible sharing permissions from server..."));
//    _ui->buttonBoxLayout->insertWidget(0, _progressIndicator);

    // Server versions >= 9.1 support the "share-permissions" property
    // older versions will just return share-permissions: ""
    auto job = new PropfindJob(accountState->account(), _sharePath);
    job->setProperties(
        QList<QByteArray>()
        << "http://open-collaboration-services.org/ns:share-permissions"
        << "http://owncloud.org/ns:fileid" // numeric file id for fallback private link generation
        << "http://owncloud.org/ns:privatelink");
    job->setTimeout(10 * 1000);
    connect(job, &PropfindJob::result, this, &ShareDialog::slotPropfindReceived);
    connect(job, &PropfindJob::finishedWithError, this, &ShareDialog::slotPropfindError);
    job->start();
}

ShareDialog::~ShareDialog()
{
    delete _ui;
}

void ShareDialog::done(int r)
{
    ConfigFile cfg;
    cfg.saveGeometry(this);
    QDialog::done(r);
}

void ShareDialog::slotPropfindReceived(const QVariantMap &result)
{
    const QVariant receivedPermissions = result["share-permissions"];
    if (!receivedPermissions.toString().isEmpty()) {
        _maxSharingPermissions = static_cast<SharePermissions>(receivedPermissions.toInt());
        qCInfo(lcSharing) << "Received sharing permissions for" << _sharePath << _maxSharingPermissions;
    }
    auto privateLinkUrl = result["privatelink"].toString();
    auto numericFileId = result["fileid"].toByteArray();
    if (!privateLinkUrl.isEmpty()) {
        qCInfo(lcSharing) << "Received private link url for" << _sharePath << privateLinkUrl;
        _privateLinkUrl = privateLinkUrl;
    } else if (!numericFileId.isEmpty()) {
        qCInfo(lcSharing) << "Received numeric file id for" << _sharePath << numericFileId;
        _privateLinkUrl = _accountState->account()->deprecatedPrivateLinkUrl(numericFileId).toString(QUrl::FullyEncoded);
    }

    showSharingUi();
}

void ShareDialog::slotPropfindError()
{
    // On error show the share ui anyway. The user can still see shares,
    // delete them and so on, even though adding new shares or granting
    // some of the permissions might fail.

    showSharingUi();
}

void ShareDialog::showSharingUi()
{
    //_progressIndicator->stopAnimation();

    auto theme = Theme::instance();

    // There's no difference between being unable to reshare and
    // being unable to reshare with reshare permission.
    bool canReshare = _maxSharingPermissions & SharePermissionShare;

    if (!canReshare) {
        auto label = new QLabel(this);
        label->setText(tr("The file can not be shared because it was shared without sharing permission."));
        label->setWordWrap(true);
        return;
    }

    // We only do user/group sharing from 8.2.0
    bool userGroupSharing =
        theme->userGroupSharing()
        && _accountState->account()->serverVersionInt() >= Account::makeServerVersion(8, 2, 0);

    if (userGroupSharing) {
        _userGroupWidget = new ShareUserGroupWidget(_accountState->account(), _sharePath, _localPath, _maxSharingPermissions, _privateLinkUrl, this);
        _ui->verticalLayout->insertWidget(1, _userGroupWidget);
        _userGroupWidget->getShares();
    }

    if (theme->linkSharing()) {
        _linkWidget = new ShareLinkWidget(_accountState->account(), _sharePath, _localPath, _maxSharingPermissions, this);
        _ui->verticalLayout->insertWidget(2, _linkWidget);
        _linkWidget->getShares();

        if (_startPage == ShareDialogStartPage::PublicLinks)
            _ui->shareWidgets->setCurrentWidget(_linkWidget);
    }
}

void ShareDialog::slotThumbnailFetched(const int &statusCode, const QByteArray &reply)
{
    if (statusCode != 200) {
        qCWarning(lcSharing) << "Thumbnail status code: " << statusCode;
        return;
    }

    QPixmap p;
    p.loadFromData(reply, "PNG");
    p = p.scaledToHeight(thumbnailSize, Qt::SmoothTransformation);
    _ui->label_icon->setPixmap(p);
    _ui->label_icon->show();
}

void ShareDialog::slotAccountStateChanged(int state)
{
    bool enabled = (state == AccountState::State::Connected);
    qCDebug(lcSharing) << "Account connected?" << enabled;

    if (_userGroupWidget != nullptr) {
        _userGroupWidget->setEnabled(enabled);
    }

    if (_linkWidget != nullptr) {
        _linkWidget->setEnabled(enabled);
    }
}
}
