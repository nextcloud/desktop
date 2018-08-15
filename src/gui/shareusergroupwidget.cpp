/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
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

#include "ui_shareusergroupwidget.h"
#include "ui_shareuserline.h"
#include "shareusergroupwidget.h"
#include "account.h"
#include "folderman.h"
#include "folder.h"
#include "accountmanager.h"
#include "theme.h"
#include "configfile.h"
#include "capabilities.h"
#include "guiutility.h"
#include "thumbnailjob.h"
#include "sharee.h"
#include "sharemanager.h"

#include "QProgressIndicator.h"
#include <QBuffer>
#include <QFileIconProvider>
#include <QClipboard>
#include <QFileInfo>
#include <QAbstractProxyModel>
#include <QCompleter>
#include <qlayout.h>
#include <QPropertyAnimation>
#include <QMenu>
#include <QAction>
#include <QDesktopServices>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QColor>
#include <QPainter>
#include <QListWidget>

namespace OCC {

ShareUserGroupWidget::ShareUserGroupWidget(AccountPtr account,
    const QString &sharePath,
    const QString &localPath,
    SharePermissions maxSharingPermissions,
    const QString &privateLinkUrl,
    QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::ShareUserGroupWidget)
    , _account(account)
    , _sharePath(sharePath)
    , _localPath(localPath)
    , _maxSharingPermissions(maxSharingPermissions)
    , _privateLinkUrl(privateLinkUrl)
    , _disableCompleterActivated(false)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("SharingDialogUG"); // required as group for saveGeometry call

    _ui->setupUi(this);

    //Is this a file or folder?
    _isFile = QFileInfo(localPath).isFile();

    _completer = new QCompleter(this);
    _completerModel = new ShareeModel(_account,
        _isFile ? QLatin1String("file") : QLatin1String("folder"),
        _completer);
    connect(_completerModel, &ShareeModel::shareesReady, this, &ShareUserGroupWidget::slotShareesReady);
    connect(_completerModel, &ShareeModel::displayErrorMessage, this, &ShareUserGroupWidget::displayError);

    _completer->setModel(_completerModel);
    _completer->setCaseSensitivity(Qt::CaseInsensitive);
    _completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    _ui->shareeLineEdit->setCompleter(_completer);

    _manager = new ShareManager(_account, this);
    connect(_manager, &ShareManager::sharesFetched, this, &ShareUserGroupWidget::slotSharesFetched);
    connect(_manager, &ShareManager::shareCreated, this, &ShareUserGroupWidget::getShares);
    connect(_manager, &ShareManager::serverError, this, &ShareUserGroupWidget::displayError);
    connect(_ui->shareeLineEdit, &QLineEdit::returnPressed, this, &ShareUserGroupWidget::slotLineEditReturn);
    //TODO connect(_ui->privateLinkText, &QLabel::linkActivated, this, &ShareUserGroupWidget::slotPrivateLinkShare);

    // By making the next two QueuedConnections we can override
    // the strings the completer sets on the line edit.
    connect(_completer, SIGNAL(activated(QModelIndex)), SLOT(slotCompleterActivated(QModelIndex)),
        Qt::QueuedConnection);
    connect(_completer, SIGNAL(highlighted(QModelIndex)), SLOT(slotCompleterHighlighted(QModelIndex)),
        Qt::QueuedConnection);

    // Queued connection so this signal is recieved after textChanged
    connect(_ui->shareeLineEdit, &QLineEdit::textEdited,
        this, &ShareUserGroupWidget::slotLineEditTextEdited, Qt::QueuedConnection);
    _ui->shareeLineEdit->installEventFilter(this);
    connect(&_completionTimer, &QTimer::timeout, this, &ShareUserGroupWidget::searchForSharees);
    _completionTimer.setSingleShot(true);
    _completionTimer.setInterval(600);

    _ui->errorLabel->hide();

    // TODO Progress Indicator where should it go?
    // Setup the sharee search progress indicator
    //_ui->shareeHorizontalLayout->addWidget(&_pi_sharee);

    _parentScrollArea = parentWidget()->findChild<QScrollArea*>("scrollArea");
}

ShareUserGroupWidget::~ShareUserGroupWidget()
{
    delete _ui;
}

void ShareUserGroupWidget::on_shareeLineEdit_textChanged(const QString &)
{
    _completionTimer.stop();
    emit togglePublicLinkShare(false);
}

void ShareUserGroupWidget::slotLineEditTextEdited(const QString &text)
{
    _disableCompleterActivated = false;
    // First textChanged is called first and we stopped the timer when the text is changed, programatically or not
    // Then we restart the timer here if the user touched a key
    if (!text.isEmpty()) {
        _completionTimer.start();
        emit togglePublicLinkShare(true);
    }
}

void ShareUserGroupWidget::slotLineEditReturn()
{
    _disableCompleterActivated = false;
    // did the user type in one of the options?
    const auto text = _ui->shareeLineEdit->text();
    for (int i = 0; i < _completerModel->rowCount(); ++i) {
        const auto sharee = _completerModel->getSharee(i);
        if (sharee->format() == text
            || sharee->displayName() == text
            || sharee->shareWith() == text) {
            slotCompleterActivated(_completerModel->index(i));
            // make sure we do not send the same item twice (because return is called when we press
            // return to activate an item inthe completer)
            _disableCompleterActivated = true;
            return;
        }
    }

    // nothing found? try to refresh completion
    _completionTimer.start();
}


void ShareUserGroupWidget::searchForSharees()
{
    _completionTimer.stop();
    _pi_sharee.startAnimation();
    ShareeModel::ShareeSet blacklist;

    // Add the current user to _sharees since we can't share with ourself
    QSharedPointer<Sharee> currentUser(new Sharee(_account->credentials()->user(), "", Sharee::Type::User));
    blacklist << currentUser;

    foreach (auto sw, _parentScrollArea->findChildren<ShareUserLine *>()) {
        blacklist << sw->share()->getShareWith();
    }
    _ui->errorLabel->hide();
    _completerModel->fetch(_ui->shareeLineEdit->text(), blacklist);
}

void ShareUserGroupWidget::getShares()
{
    _manager->fetchShares(_sharePath);
}

void ShareUserGroupWidget::slotSharesFetched(const QList<QSharedPointer<Share>> &shares)
{
    QScrollArea *scrollArea = _parentScrollArea;

    auto newViewPort = new QWidget(scrollArea);
    auto layout = new QVBoxLayout(newViewPort);
    QSize minimumSize = newViewPort->sizeHint();
    int x = 0;

    foreach (const auto &share, shares) {
        // We don't handle link shares, only TypeUser or TypeGroup
        if (share->getShareType() == Share::TypeLink) {
            continue;
        }

        ShareUserLine *s = new ShareUserLine(share, _maxSharingPermissions, _isFile, _parentScrollArea);
        connect(s, &ShareUserLine::resizeRequested, this, &ShareUserGroupWidget::slotAdjustScrollWidgetSize);
        connect(s, &ShareUserLine::visualDeletionDone, this, &ShareUserGroupWidget::getShares);
        s->setBackgroundRole(layout->count() % 2 == 0 ? QPalette::Base : QPalette::AlternateBase);
        layout->addWidget(s);

        x++;
        if (x <= 3) {
            minimumSize = newViewPort->sizeHint();
        } else {
            minimumSize.rwidth() = qMax(newViewPort->sizeHint().width(), minimumSize.width());
        }
    }

    if (!layout->isEmpty()) {
        _parentScrollArea->setVisible(true);
        layout->addStretch(1);
    } else {
        _parentScrollArea->setVisible(false);
    }

    minimumSize.rwidth() += layout->spacing();
    minimumSize.rheight() += layout->spacing();
    scrollArea->setMinimumSize(minimumSize);
    scrollArea->setWidget(newViewPort);

    _disableCompleterActivated = false;
    _ui->shareeLineEdit->setEnabled(true);
}

void ShareUserGroupWidget::slotAdjustScrollWidgetSize()
{
    QScrollArea *scrollArea = _parentScrollArea;
    if (scrollArea->findChildren<ShareUserLine *>().count() <= 3 &&
            scrollArea->findChildren<ShareUserLine *>().count() > 0) {
        auto minimumSize = scrollArea->widget()->sizeHint();
        auto spacing = scrollArea->widget()->layout()->spacing();
        minimumSize.rwidth() += spacing;
        minimumSize.rheight() += spacing;
        scrollArea->setMinimumSize(minimumSize);
    }
}

void ShareUserGroupWidget::slotPrivateLinkShare()
{
    auto menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(QIcon(":/client/resources/copy.svg"),
                    tr("Copy link"),
        this, SLOT(slotPrivateLinkCopy()));

    menu->exec(QCursor::pos());
}

void ShareUserGroupWidget::slotShareesReady()
{
    _pi_sharee.stopAnimation();
    if (_completerModel->rowCount() == 0) {
        displayError(0, tr("No results for '%1'").arg(_completerModel->currentSearch()));
        return;
    }
    _completer->complete();
}

void ShareUserGroupWidget::slotCompleterActivated(const QModelIndex &index)
{
    if (_disableCompleterActivated)
        return;
    // The index is an index from the QCompletion model which is itelf a proxy
    // model proxying the _completerModel
    auto sharee = qvariant_cast<QSharedPointer<Sharee>>(index.data(Qt::UserRole));
    if (sharee.isNull()) {
        _parentScrollArea->setVisible(false);
        return;
    }

    /*
     * Add spinner to the bottom of the widget list
     */
    auto viewPort = _parentScrollArea->widget();
    auto layout = qobject_cast<QVBoxLayout *>(viewPort->layout());

// TODO Progress Indicator where should it go?
//    auto indicator = new QProgressIndicator(viewPort);
//    indicator->startAnimation();
//    if (layout->count() == 1) {
//        // No shares yet! Remove the label, add some stretch.
//        delete layout->itemAt(0)->widget();
//        layout->addStretch(1);
//    }
//    layout->insertWidget(layout->count() - 1, indicator);

    /*
     * Don't send the reshare permissions for federated shares for servers <9.1
     * https://github.com/owncloud/core/issues/22122#issuecomment-185637344
     * https://github.com/owncloud/client/issues/4996
     */
    if (sharee->type() == Sharee::Federated
        && _account->serverVersionInt() < Account::makeServerVersion(9, 1, 0)) {
        int permissions = SharePermissionRead | SharePermissionUpdate;
        if (!_isFile) {
            permissions |= SharePermissionCreate | SharePermissionDelete;
        }
        _manager->createShare(_sharePath, Share::ShareType(sharee->type()),
            sharee->shareWith(), SharePermission(permissions));
    } else {

        // Default permissions on creation
        int permissions = SharePermissionRead | SharePermissionUpdate;
        _manager->createShare(_sharePath, Share::ShareType(sharee->type()),
            sharee->shareWith(), SharePermission(permissions));
    }

    _ui->shareeLineEdit->setEnabled(false);
    _ui->shareeLineEdit->setText(QString());

    if(layout->isEmpty())
        _parentScrollArea->setVisible(false);
    else
        _parentScrollArea->setVisible(true);
}

void ShareUserGroupWidget::slotCompleterHighlighted(const QModelIndex &index)
{
    // By default the completer would set the text to EditRole,
    // override that here.
    _ui->shareeLineEdit->setText(index.data(Qt::DisplayRole).toString());
}

void ShareUserGroupWidget::displayError(int code, const QString &message)
{
    _pi_sharee.stopAnimation();

    // Also remove the spinner in the widget list, if any
    foreach (auto pi, _parentScrollArea->findChildren<QProgressIndicator *>()) {
        delete pi;
    }

    qCWarning(lcSharing) << "Sharing error from server" << code << message;
    _ui->errorLabel->setText(message);
    _ui->errorLabel->show();
    _ui->shareeLineEdit->setEnabled(true);
}

void ShareUserGroupWidget::slotPrivateLinkOpenBrowser()
{
    Utility::openBrowser(_privateLinkUrl, this);
}

void ShareUserGroupWidget::slotPrivateLinkCopy()
{
    QApplication::clipboard()->setText(_privateLinkUrl);
}

void ShareUserGroupWidget::slotPrivateLinkEmail()
{
    Utility::openEmailComposer(
        tr("I shared something with you"),
        _privateLinkUrl,
        this);
}

ShareUserLine::ShareUserLine(QSharedPointer<Share> share,
    SharePermissions maxSharingPermissions,
    bool isFile,
    QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::ShareUserLine)
    , _share(share)
    , _isFile(isFile)
{
    _ui->setupUi(this);

    QString sharedWithText(share->getShareWith()->format());
    QFontMetrics metrics(_ui->sharedWith->font());
    QString elidedText = metrics.elidedText(sharedWithText, Qt::ElideRight, _ui->sharedWith->width());
    _ui->sharedWith->setText(elidedText);

    // adds permissions
    // can edit permission
    bool enabled = maxSharingPermissions & (SharePermissionRead & SharePermissionUpdate);
    if(!_isFile) enabled = enabled & (SharePermissionCreate & SharePermissionDelete);
    _ui->permissionsEdit->setEnabled(enabled);
    connect(_ui->permissionsEdit, &QAbstractButton::clicked, this, &ShareUserLine::slotEditPermissionsChanged);

    // create menu with checkable permissions
    QMenu *menu = new QMenu(this);
    _permissionReshare= new QAction(tr("Can reshare"), this);
    _permissionReshare->setCheckable(true);
    _permissionReshare->setEnabled(maxSharingPermissions & SharePermissionShare);
    menu->addAction(_permissionReshare);
    connect(_permissionReshare, &QAction::triggered, this, &ShareUserLine::slotPermissionsChanged);

    /*
     * Files can't have create or delete permissions
     */
    if (!_isFile) {
        _permissionCreate = new QAction(tr("Can create"), this);
        _permissionCreate->setCheckable(true);
        _permissionCreate->setEnabled(maxSharingPermissions & SharePermissionCreate);
        menu->addAction(_permissionCreate);
        connect(_permissionCreate, &QAction::triggered, this, &ShareUserLine::slotPermissionsChanged);

        _permissionChange = new QAction(tr("Can change"), this);
        _permissionChange->setCheckable(true);
        _permissionChange->setEnabled(maxSharingPermissions & SharePermissionUpdate);
        menu->addAction(_permissionChange);
        connect(_permissionChange, &QAction::triggered, this, &ShareUserLine::slotPermissionsChanged);

        _permissionDelete = new QAction(tr("Can delete"), this);
        _permissionDelete->setCheckable(true);
        _permissionDelete->setEnabled(maxSharingPermissions & SharePermissionDelete);
        menu->addAction(_permissionDelete);
        connect(_permissionDelete, &QAction::triggered, this, &ShareUserLine::slotPermissionsChanged);
    }

    _ui->permissionToolButton->setMenu(menu);
    _ui->permissionToolButton->setPopupMode(QToolButton::InstantPopup);

    QIcon icon(QLatin1String(":/client/resources/more.svg"));
    _ui->permissionToolButton->setIcon(icon);

    // Set the permissions checkboxes
    displayPermissions();

    /*
     * We don't show permission share for federated shares with server <9.1
     * https://github.com/owncloud/core/issues/22122#issuecomment-185637344
     * https://github.com/owncloud/client/issues/4996
     */
    if (share->getShareType() == Share::TypeRemote
        && share->account()->serverVersionInt() < Account::makeServerVersion(9, 1, 0)) {
        _permissionReshare->setVisible(false);
        _ui->permissionToolButton->setVisible(false);
    }

    connect(share.data(), &Share::permissionsSet, this, &ShareUserLine::slotPermissionsSet);
    connect(share.data(), &Share::shareDeleted, this, &ShareUserLine::slotShareDeleted);

    _ui->deleteShareButton->setIcon(QIcon::fromTheme(QLatin1String("user-trash"),
                                                     QIcon(QLatin1String(":/client/resources/delete.png"))));

    if (!share->account()->capabilities().shareResharing()) {
        _permissionReshare->setVisible(false);
    }

    loadAvatar();
}

void ShareUserLine::loadAvatar()
{
    const int avatarSize = 36;

    // Set size of the placeholder
    _ui->avatar->setMinimumHeight(avatarSize);
    _ui->avatar->setMinimumWidth(avatarSize);
    _ui->avatar->setMaximumHeight(avatarSize);
    _ui->avatar->setMaximumWidth(avatarSize);
    _ui->avatar->setAlignment(Qt::AlignCenter);

    /* Create the fallback avatar.
     *
     * This will be shown until the avatar image data arrives.
     */
    const QByteArray hash = QCryptographicHash::hash(_ui->sharedWith->text().toUtf8(), QCryptographicHash::Md5);
    double hue = static_cast<quint8>(hash[0]) / 255.;

    // See core/js/placeholder.js for details on colors and styling
    const QColor bg = QColor::fromHslF(hue, 0.7, 0.68);
    const QString style = QString(R"(* {
        color: #fff;
        background-color: %1;
        border-radius: %2px;
        text-align: center;
        line-height: %2px;
        font-size: %2px;
    })").arg(bg.name(), QString::number(avatarSize / 2));
    _ui->avatar->setStyleSheet(style);

    // The avatar label is the first character of the user name.
    const QString text = _share->getShareWith()->displayName();
    _ui->avatar->setText(text.at(0).toUpper());

    /* Start the network job to fetch the avatar data.
     *
     * Currently only regular users can have avatars.
     */
    if (_share->getShareWith()->type() == Sharee::User) {
        AvatarJob *job = new AvatarJob(_share->account(), _share->getShareWith()->shareWith(), avatarSize, this);
        connect(job, &AvatarJob::avatarPixmap, this, &ShareUserLine::slotAvatarLoaded);
        job->start();
    }
}

void ShareUserLine::slotAvatarLoaded(QImage avatar)
{
    if (avatar.isNull())
        return;

    avatar = AvatarJob::makeCircularAvatar(avatar);
    _ui->avatar->setPixmap(QPixmap::fromImage(avatar));

    // Remove the stylesheet for the fallback avatar
    _ui->avatar->setStyleSheet("");
}

void ShareUserLine::on_deleteShareButton_clicked()
{
    setEnabled(false);
    _share->deleteShare();
}

ShareUserLine::~ShareUserLine()
{
    delete _ui;
}

void ShareUserLine::slotEditPermissionsChanged()
{
    setEnabled(false);

    // Can never manually be set to "partial".
    // This works because the state cycle for clicking is
    // unchecked -> partial -> checked -> unchecked.
    if (_ui->permissionsEdit->checkState() == Qt::PartiallyChecked) {
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }

    Share::Permissions permissions = SharePermissionRead;

    if (_permissionReshare->isChecked()) {
        permissions |= SharePermissionShare;
    }

    if (_ui->permissionsEdit->checkState() == Qt::Checked) {
        if (_permissionChange->isEnabled())
            permissions |= SharePermissionUpdate;

        /*
         * Files can't have create or delete permisisons
         */
        if (!_isFile) {
            if (_permissionCreate->isEnabled())
                permissions |= SharePermissionCreate;
            if (_permissionDelete->isEnabled())
                permissions |= SharePermissionDelete;
        }
    }

    _share->setPermissions(permissions);
}

void ShareUserLine::slotPermissionsChanged()
{
    setEnabled(false);

    Share::Permissions permissions = SharePermissionRead;

    if (_permissionReshare->isChecked()) {
        permissions |= SharePermissionShare;
    }

    if (!_isFile) {
        if (_permissionCreate->isChecked()) {
            permissions |= SharePermissionCreate;
        }

        if (_permissionChange->isChecked()) {
            permissions |= SharePermissionUpdate;
        }

        if (_permissionDelete->isChecked()) {
            permissions |= SharePermissionDelete;
        }
     }

    _share->setPermissions(permissions);
}

void ShareUserLine::slotDeleteAnimationFinished()
{
    emit resizeRequested();
    emit visualDeletionDone();
    deleteLater();

    // There is a painting bug where a small line of this widget isn't
    // properly cleared. This explicit repaint() call makes sure any trace of
    // the share widget is removed once it's destroyed. #4189
    connect(this, SIGNAL(destroyed(QObject *)), parentWidget(), SLOT(repaint()));
}

void ShareUserLine::slotShareDeleted()
{
    QPropertyAnimation *animation = new QPropertyAnimation(this, "maximumHeight", this);

    animation->setDuration(500);
    animation->setStartValue(height());
    animation->setEndValue(0);

    connect(animation, &QAbstractAnimation::finished, this, &ShareUserLine::slotDeleteAnimationFinished);
    connect(animation, &QVariantAnimation::valueChanged, this, &ShareUserLine::resizeRequested);

    animation->start();
}

void ShareUserLine::slotPermissionsSet()
{
    displayPermissions();
    setEnabled(true);
}

QSharedPointer<Share> ShareUserLine::share() const
{
    return _share;
}

void ShareUserLine::displayPermissions()
{
    auto perm = _share->getPermissions();

    if (perm & SharePermissionUpdate
        && (_isFile
               || (perm & SharePermissionCreate
                      && perm & SharePermissionDelete))) {
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    } else if (perm & (SharePermissionUpdate | SharePermissionCreate | SharePermissionDelete)) {
        _ui->permissionsEdit->setCheckState(Qt::PartiallyChecked);
    } else {
        _ui->permissionsEdit->setCheckState(Qt::Unchecked);
    }

    _permissionReshare->setChecked(Qt::Unchecked);
    if (perm & SharePermissionShare) {
        _permissionReshare->setChecked(Qt::Checked);
    }

    if(!_isFile){
        _permissionCreate->setChecked(perm & SharePermissionCreate);
        _permissionChange->setChecked(perm & SharePermissionUpdate);
        _permissionDelete->setChecked(perm & SharePermissionDelete);
    }
}
}
