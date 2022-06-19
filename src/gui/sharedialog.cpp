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

#include "ui_sharedialog.h"
#include "sharedialog.h"
#include "sharee.h"
#include "sharelinkwidget.h"
#include "internallinkwidget.h"
#include "shareusergroupwidget.h"
#include "passwordinputdialog.h"

#include "sharemanager.h"

#include "account.h"
#include "accountstate.h"
#include "configfile.h"
#include "theme.h"
#include "thumbnailjob.h"
#include "wordlist.h"

#include <QFileInfo>
#include <QFileIconProvider>
#include <QInputDialog>
#include <QPointer>
#include <QPushButton>
#include <QFrame>

namespace {
QString createRandomPassword()
{
    const auto words = OCC::WordList::getRandomWords(10);

    const auto addFirstLetter = [](const QString &current, const QString &next) -> QString {
        return current + next.at(0);
    };

    return std::accumulate(std::cbegin(words), std::cend(words), QString(), addFirstLetter);
}
}


namespace OCC {

static const int thumbnailSize = 40;

ShareDialog::ShareDialog(QPointer<AccountState> accountState,
    const QString &sharePath,
    const QString &localPath,
    SharePermissions maxSharingPermissions,
    const QByteArray &numericFileId,
    SyncJournalFileLockInfo filelockState,
    ShareDialogStartPage startPage,
    QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::ShareDialog)
    , _accountState(accountState)
    , _sharePath(sharePath)
    , _localPath(localPath)
    , _maxSharingPermissions(maxSharingPermissions)
    , _filelockState(std::move(filelockState))
    , _privateLinkUrl(accountState->account()->deprecatedPrivateLinkUrl(numericFileId).toString(QUrl::FullyEncoded))
    , _startPage(startPage)
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
    QString fileName = QFileInfo(_sharePath).fileName();
    _ui->label_name->setText(tr("%1").arg(fileName));
    QFont f(_ui->label_name->font());
    f.setPointSize(qRound(f.pointSize() * 1.4));
    _ui->label_name->setFont(f);

    if (_filelockState._locked) {
        static constexpr auto SECONDS_PER_MINUTE = 60;
        const auto lockExpirationTime = _filelockState._lockTime + _filelockState._lockTimeout;
        const auto remainingTime = QDateTime::currentDateTime().secsTo(QDateTime::fromSecsSinceEpoch(lockExpirationTime));
        const auto remainingTimeInMinute = static_cast<int>(remainingTime > 0 ? remainingTime / SECONDS_PER_MINUTE : 0);
        _ui->label_lockinfo->setText(tr("Locked by %1 - Expires in %2 minutes", "remaining time before lock expires", remainingTimeInMinute).arg(_filelockState._lockOwnerDisplayName).arg(remainingTimeInMinute));
    } else {
        _ui->label_lockinfo->setVisible(false);
    }

    QString ocDir(_sharePath);
    ocDir.truncate(ocDir.length() - fileName.length());

    ocDir.replace(QRegularExpression("^/*"), "");
    ocDir.replace(QRegularExpression("/*$"), "");

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
        return;
    }

    if (QFileInfo(_localPath).isFile()) {
        auto *job = new ThumbnailJob(_sharePath, _accountState->account(), this);
        connect(job, &ThumbnailJob::jobFinished, this, &ShareDialog::slotThumbnailFetched);
        job->start();
    }

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

    initShareManager();
    
    _scrollAreaViewPort = new QWidget(_ui->scrollArea);
    _scrollAreaLayout = new QVBoxLayout(_scrollAreaViewPort);
    _scrollAreaLayout->setContentsMargins(0, 0, 0, 0);
    _ui->scrollArea->setWidget(_scrollAreaViewPort);

    _internalLinkWidget = new InternalLinkWidget(localPath, this);
    _ui->verticalLayout->addWidget(_internalLinkWidget);
    _internalLinkWidget->setupUiOptions();
    connect(this, &ShareDialog::styleChanged, _internalLinkWidget, &InternalLinkWidget::slotStyleChanged);
}

ShareLinkWidget *ShareDialog::addLinkShareWidget(const QSharedPointer<LinkShare> &linkShare)
{
    const auto linkShareWidget = new ShareLinkWidget(_accountState->account(), _sharePath, _localPath, _maxSharingPermissions, _ui->scrollArea);
    _linkWidgetList.append(linkShareWidget);

    linkShareWidget->setLinkShare(linkShare);

    connect(linkShare.data(), &Share::serverError, linkShareWidget, &ShareLinkWidget::slotServerError);
    connect(linkShare.data(), &Share::shareDeleted, linkShareWidget, &ShareLinkWidget::slotDeleteShareFetched);

    if(_manager) {
        connect(_manager, &ShareManager::serverError, linkShareWidget, &ShareLinkWidget::slotServerError);
    }

    // Connect all shares signals to gui slots
    connect(this, &ShareDialog::toggleShareLinkAnimation, linkShareWidget, &ShareLinkWidget::slotToggleShareLinkAnimation);
    connect(linkShareWidget, &ShareLinkWidget::createLinkShare, this, &ShareDialog::slotCreateLinkShare);
    connect(linkShareWidget, &ShareLinkWidget::deleteLinkShare, this, &ShareDialog::slotDeleteShare);
    connect(linkShareWidget, &ShareLinkWidget::createPassword, this, &ShareDialog::slotCreatePasswordForLinkShare);
    
    // Connect styleChanged events to our widget, so it can adapt (Dark-/Light-Mode switching)
    connect(this, &ShareDialog::styleChanged, linkShareWidget, &ShareLinkWidget::slotStyleChanged);

    _ui->verticalLayout->insertWidget(_linkWidgetList.size() + 1, linkShareWidget);
    _scrollAreaLayout->addWidget(linkShareWidget);
    
    linkShareWidget->setupUiOptions();

    return linkShareWidget;
}

void ShareDialog::initLinkShareWidget()
{
    if(_linkWidgetList.size() == 0) {
        _emptyShareLinkWidget = new ShareLinkWidget(_accountState->account(), _sharePath, _localPath, _maxSharingPermissions, _ui->scrollArea);
        _linkWidgetList.append(_emptyShareLinkWidget);

        _emptyShareLinkWidget->slotStyleChanged(); // Get the initial customizeStyle() to happen

        connect(this, &ShareDialog::toggleShareLinkAnimation, _emptyShareLinkWidget, &ShareLinkWidget::slotToggleShareLinkAnimation);
        connect(this, &ShareDialog::styleChanged, _emptyShareLinkWidget, &ShareLinkWidget::slotStyleChanged);

        connect(_emptyShareLinkWidget, &ShareLinkWidget::createLinkShare, this, &ShareDialog::slotCreateLinkShare);
        connect(_emptyShareLinkWidget, &ShareLinkWidget::createPassword, this, &ShareDialog::slotCreatePasswordForLinkShare);

        _ui->verticalLayout->insertWidget(_linkWidgetList.size()+1, _emptyShareLinkWidget);
        _scrollAreaLayout->addWidget(_emptyShareLinkWidget);
        _emptyShareLinkWidget->show();
    } else if (_emptyShareLinkWidget) {
        _emptyShareLinkWidget->hide();
        _ui->verticalLayout->removeWidget(_emptyShareLinkWidget);
        _linkWidgetList.removeAll(_emptyShareLinkWidget);
        _emptyShareLinkWidget = nullptr;
    }
}

void ShareDialog::slotAddLinkShareWidget(const QSharedPointer<LinkShare> &linkShare)
{
    emit toggleShareLinkAnimation(true);
    const auto addedLinkShareWidget = addLinkShareWidget(linkShare);
    initLinkShareWidget();
    adjustScrollWidgetSize();
    if (linkShare->isPasswordSet()) {
        addedLinkShareWidget->focusPasswordLineEdit();
    }
    emit toggleShareLinkAnimation(false);
}

void ShareDialog::slotSharesFetched(const QList<QSharedPointer<Share>> &shares)
{
    emit toggleShareLinkAnimation(true);

    const QString versionString = _accountState->account()->serverVersion();
    qCInfo(lcSharing) << versionString << "Fetched" << shares.count() << "shares";
    
    foreach (auto share, shares) {
        if (share->getShareType() != Share::TypeLink || share->getUidOwner() != share->account()->davUser()) {
            continue;
        }

        QSharedPointer<LinkShare> linkShare = qSharedPointerDynamicCast<LinkShare>(share);
        addLinkShareWidget(linkShare);
    }

    initLinkShareWidget();
    adjustScrollWidgetSize();
    emit toggleShareLinkAnimation(false);
}

void ShareDialog::adjustScrollWidgetSize()
{
    const auto count = _scrollAreaLayout->count();
    const auto margin = 10;
    const auto height = _linkWidgetList.empty() ? 0 : _linkWidgetList.last()->sizeHint().height() + margin;
    const auto totalHeight = height * count;
    _ui->scrollArea->setFixedWidth(_ui->verticalLayout->sizeHint().width());
    _ui->scrollArea->setFixedHeight(totalHeight > 400 ? 400 : totalHeight);
    _ui->scrollArea->setVisible(height > 0);
    _ui->scrollArea->setFrameShape(count > 6 ? QFrame::StyledPanel : QFrame::NoFrame);
}

ShareDialog::~ShareDialog()
{
    _linkWidgetList.clear();
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
    auto theme = Theme::instance();

    // There's no difference between being unable to reshare and
    // being unable to reshare with reshare permission.
    bool canReshare = _maxSharingPermissions & SharePermissionShare;

    if (!canReshare) {
        auto label = new QLabel(this);
        label->setText(tr("The file cannot be shared because it does not have sharing permission."));
        label->setWordWrap(true);
        _ui->verticalLayout->insertWidget(1, label);
        return;
    }

    if (theme->userGroupSharing()) {
        _userGroupWidget = new ShareUserGroupWidget(_accountState->account(), _sharePath, _localPath, _maxSharingPermissions, _privateLinkUrl, _ui->scrollArea);
        _userGroupWidget->getShares();
        
        // Connect styleChanged events to our widget, so it can adapt (Dark-/Light-Mode switching)
        connect(this, &ShareDialog::styleChanged, _userGroupWidget, &ShareUserGroupWidget::slotStyleChanged);

        _userGroupWidget->slotStyleChanged();

        _ui->verticalLayout->insertWidget(1, _userGroupWidget);
        _scrollAreaLayout->addLayout(_userGroupWidget->shareUserGroupLayout());
    }

    initShareManager();

    if (theme->linkSharing()) {
        if(_manager) {
            _manager->fetchShares(_sharePath);
        }
    }
}

void ShareDialog::initShareManager()
{
    bool sharingPossible = true;
    if (!_accountState->account()->capabilities().sharePublicLink()) {
        qCWarning(lcSharing) << "Link shares have been disabled";
        sharingPossible = false;
    } else if (!(_maxSharingPermissions & SharePermissionShare)) {
        qCWarning(lcSharing) << "The file cannot be shared because it does not have sharing permission.";
        sharingPossible = false;
    }

    if (!_manager && sharingPossible) {
        _manager = new ShareManager(_accountState->account(), this);
        connect(_manager, &ShareManager::sharesFetched, this, &ShareDialog::slotSharesFetched);
        connect(_manager, &ShareManager::linkShareCreated, this, &ShareDialog::slotAddLinkShareWidget);
        connect(_manager, &ShareManager::linkShareRequiresPassword, this, &ShareDialog::slotLinkShareRequiresPassword);
    }
}

void ShareDialog::slotCreateLinkShare()
{
    if(_manager) {
        const auto askOptionalPassword = _accountState->account()->capabilities().sharePublicLinkAskOptionalPassword();
        const auto password = askOptionalPassword ? createRandomPassword() : QString();
        _manager->createLinkShare(_sharePath, QString(), password);
    }
}

void ShareDialog::slotCreatePasswordForLinkShare(const QString &password)
{
    const auto shareLinkWidget = qobject_cast<ShareLinkWidget*>(sender());
    Q_ASSERT(shareLinkWidget);
    if (shareLinkWidget) {
        connect(_manager, &ShareManager::linkShareRequiresPassword, shareLinkWidget, &ShareLinkWidget::slotCreateShareRequiresPassword);
        connect(shareLinkWidget, &ShareLinkWidget::createPasswordProcessed, this, &ShareDialog::slotCreatePasswordForLinkShareProcessed);
        shareLinkWidget->getLinkShare()->setPassword(password);
    } else {
        qCCritical(lcSharing) << "shareLinkWidget is not a sender!";
    }
}

void ShareDialog::slotCreatePasswordForLinkShareProcessed()
{
    const auto shareLinkWidget = qobject_cast<ShareLinkWidget*>(sender());
    Q_ASSERT(shareLinkWidget);
    if (shareLinkWidget) {
        disconnect(_manager, &ShareManager::linkShareRequiresPassword, shareLinkWidget, &ShareLinkWidget::slotCreateShareRequiresPassword);
        disconnect(shareLinkWidget, &ShareLinkWidget::createPasswordProcessed, this, &ShareDialog::slotCreatePasswordForLinkShareProcessed);
    } else {
        qCCritical(lcSharing) << "shareLinkWidget is not a sender!";
    }
}

void ShareDialog::slotLinkShareRequiresPassword(const QString &message)
{
    const auto passwordInputDialog = new PasswordInputDialog(tr("Please enter a password for your link share:"), message, this);
    passwordInputDialog->setWindowTitle(tr("Password for share required"));
    passwordInputDialog->setAttribute(Qt::WA_DeleteOnClose);
    passwordInputDialog->open();

    connect(passwordInputDialog, &QDialog::finished, this, [this, passwordInputDialog](const int result) {
        if (result == QDialog::Accepted && _manager) {
            // Try to create the link share again with the newly entered password
            _manager->createLinkShare(_sharePath, QString(), passwordInputDialog->password());
            return;
        }
        emit toggleShareLinkAnimation(false);
    });
}

void ShareDialog::slotDeleteShare()
{
    auto sharelinkWidget = dynamic_cast<ShareLinkWidget*>(sender());
    sharelinkWidget->hide();
    _ui->verticalLayout->removeWidget(sharelinkWidget);
    _scrollAreaLayout->removeWidget(sharelinkWidget);
    _linkWidgetList.removeAll(sharelinkWidget);
    initLinkShareWidget();
    adjustScrollWidgetSize();
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

    if (_userGroupWidget) {
        _userGroupWidget->setEnabled(enabled);
    }

    if(_linkWidgetList.size() > 0){
        foreach(ShareLinkWidget *widget, _linkWidgetList){
            widget->setEnabled(state);
        }
    }
}

void ShareDialog::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        // Notify the other widgets (Dark-/Light-Mode switching)
        emit styleChanged();
        break;
    default:
        break;
    }

    QDialog::changeEvent(e);
}

} // namespace OCC
