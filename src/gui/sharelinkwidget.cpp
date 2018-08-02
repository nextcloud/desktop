/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 * Copyright (C) 2015 by Klaas Freitag <freitag@owncloud.com>
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

#include "sharelinkwidget.h"
#include "ui_sharelinkwidget.h"
#include "account.h"
#include "capabilities.h"

#include "sharemanager.h"
#include "guiutility.h"

#include "QProgressIndicator.h"
#include <QBuffer>
#include <QClipboard>
#include <QFileInfo>
#include <QDesktopServices>
#include <QMessageBox>
#include <QMenu>
#include <QToolButton>

namespace OCC {

ShareLinkWidget::ShareLinkWidget(AccountPtr account,
    const QString &sharePath,
    const QString &localPath,
    SharePermissions maxSharingPermissions,
    QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::ShareLinkWidget)
    , _account(account)
    , _sharePath(sharePath)
    , _localPath(localPath)
    , _manager(nullptr)
    , _linkShare(nullptr)
    , _passwordRequired(false)
    , _expiryRequired(false)
    , _namesSupported(true)
    , _linkContextMenu(nullptr)
    , _copyLinkAction(nullptr)
    , _readOnlyLinkAction(nullptr)
    , _allowEditingLinkAction(nullptr)
    , _allowUploadEditingLinkAction(nullptr)
    , _allowUploadLinkAction(nullptr)
    , _passwordProtectLinkAction(nullptr)
    , _expirationDateLinkAction(nullptr)
    , _unshareLinkAction(nullptr)
{
    _ui->setupUi(this);

    //Is this a file or folder?
    QFileInfo fi(localPath);
    _isFile = fi.isFile();

    // the following progress indicator widgets are added to layouts which makes them
    // automatically deleted once the dialog dies.
    _pi_create = new QProgressIndicator();
    _pi_password = new QProgressIndicator();
    _pi_date = new QProgressIndicator();
    _pi_editing = new QProgressIndicator();

// TODO: where to loading should show up?
//    _ui->verticalLayout->addWidget(_pi_create, Qt::AlignCenter);
//    _ui->verticalLayout->addWidget(_pi_password, Qt::AlignCenter);
//    _ui->verticalLayout->addWidget(_pi_editing, Qt::AlignCenter);

    connect(_ui->enableShareLink, &QCheckBox::toggled, this, &ShareLinkWidget::slotCreateorDeleteShareLink);
    connect(_ui->lineEdit_password, &QLineEdit::returnPressed, this, &ShareLinkWidget::slotCreatePassword);
    connect(_ui->confirmPassword, &QAbstractButton::clicked, this, &ShareLinkWidget::slotCreatePassword);
    connect(_ui->confirmExpirationDate, &QAbstractButton::clicked, this, &ShareLinkWidget::slotCreatePassword);
    connect(_ui->calendar, &QDateTimeEdit::dateChanged, this, &ShareLinkWidget::slotExpireDateChanged);

    _ui->errorLabel->hide();

    bool sharingPossible = true;
    if (!_account->capabilities().sharePublicLink()) {
        qCWarning(lcSharing) << "Link shares have been disabled";
        sharingPossible = false;
    } else if (!(maxSharingPermissions & SharePermissionShare)) {
        qCWarning(lcSharing) << "The file can not be shared because it was shared without sharing permission.";
        sharingPossible = false;
    }

    if (!sharingPossible)
        _ui->shareLinkWidget->hide();
    else
        _ui->shareLinkWidget->show();

    // Older servers don't support multiple public link shares
    if (!_account->capabilities().sharePublicLinkMultiple()) {
        _namesSupported = false;
    }

    _ui->passwordShareProperty->hide();
    _ui->expirationShareProperty->hide();
    _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));

    // check if the file is already inside of a synced folder
    if (sharePath.isEmpty()) {
        // The file is not yet in an ownCloud synced folder. We could automatically
        // copy it over, but that is skipped as not all questions can be answered that
        // are involved in that, see https://github.com/owncloud/client/issues/2732
        //
        // _ui->checkBox_shareLink->setEnabled(false);
        // uploadExternalFile();
        qCWarning(lcSharing) << "Unable to share files not in a sync folder.";
        return;
    }


    // TODO File Drop
    // File can't have public upload set; we also hide it if the capability isn't there
//    _ui->widget_editing->setVisible(
//        !_isFile && _account->capabilities().sharePublicLinkAllowUpload());
    //_ui->radio_uploadOnly->setVisible(
        //_account->capabilities().sharePublicLinkSupportsUploadOnly());

    /*
     * Create the share manager and connect it properly
     */
    if (sharingPossible) {
        _manager = new ShareManager(_account, this);
        connect(_manager, &ShareManager::sharesFetched, this, &ShareLinkWidget::slotSharesFetched);
        connect(_manager, &ShareManager::linkShareCreated, this, &ShareLinkWidget::slotCreateShareFetched);
        connect(_manager, &ShareManager::linkShareRequiresPassword, this, &ShareLinkWidget::slotCreateShareRequiresPassword);
        connect(_manager, &ShareManager::serverError, this, &ShareLinkWidget::slotServerError);
    }
}

ShareLinkWidget::~ShareLinkWidget()
{
    delete _ui;
}

void ShareLinkWidget::getShares()
{
    if (_manager) {
        _manager->fetchShares(_sharePath);
    }
}

void ShareLinkWidget::slotSharesFetched(const QList<QSharedPointer<Share>> &shares)
{
    const QString versionString = _account->serverVersion();
    qCInfo(lcSharing) << versionString << "Fetched" << shares.count() << "shares";

    foreach (auto share, shares) {
        if (share->getShareType() != Share::TypeLink) {
            continue;
        }
        _linkShare = qSharedPointerDynamicCast<LinkShare>(share);

        // Connect all shares signals to gui slots
        connect(share.data(), &Share::serverError, this, &ShareLinkWidget::slotServerError);
        connect(share.data(), &Share::shareDeleted, this, &ShareLinkWidget::slotDeleteShareFetched);
        //TODO connect(_linkShare.data(), &LinkShare::expireDateSet, this, &ShareLinkWidget::slotExpireSet);
        connect(_linkShare.data(), &LinkShare::passwordSet, this, &ShareLinkWidget::slotPasswordSet);
        connect(_linkShare.data(), &LinkShare::passwordSetError, this, &ShareLinkWidget::slotPasswordSetError);

        // Prepare permissions check and create group action
        bool checked = false;
        SharePermissions perm = _linkShare->getPermissions();
        QActionGroup *permissionsGroup = new QActionGroup(this);

        // radio button style
        permissionsGroup->setExclusive(true);

        if(_isFile){
            checked = perm & (SharePermissionRead & SharePermissionUpdate);
            _allowEditingLinkAction = permissionsGroup->addAction(tr("Allow Editing"));
            _allowEditingLinkAction->setCheckable(true);
            _allowEditingLinkAction->setChecked(checked);

        } else {
            checked = perm & SharePermissionRead;
            _readOnlyLinkAction = permissionsGroup->addAction(tr("Read only"));
            _readOnlyLinkAction->setCheckable(true);
            _readOnlyLinkAction->setChecked(checked);

            checked = perm & (SharePermissionRead &
                              SharePermissionCreate &
                              SharePermissionUpdate &
                              SharePermissionDelete);
            _allowUploadEditingLinkAction = permissionsGroup->addAction(tr("Allow Upload && Editing"));
            _allowUploadEditingLinkAction->setCheckable(true);
            _allowUploadEditingLinkAction->setChecked(checked);

            checked = perm & SharePermissionCreate;
            _allowUploadLinkAction = permissionsGroup->addAction(tr("File Drop (Upload Only)"));
            _allowUploadLinkAction->setCheckable(true);
            _allowUploadLinkAction->setChecked(checked);
        }

        // Prepare sharing menu
        _linkContextMenu = new QMenu(this);

        // Add copy action (icon only)
        _copyLinkAction = _linkContextMenu->addAction(QIcon(":/client/resources/copy.svg"),
                                                      tr("Copy link"));

        // Adds permissions actions (radio button style)
        if(_isFile){
            _linkContextMenu->addAction(_allowEditingLinkAction);
        } else {
            _linkContextMenu->addAction(_readOnlyLinkAction);
            _linkContextMenu->addAction(_allowUploadEditingLinkAction);
            _linkContextMenu->addAction(_allowUploadLinkAction);
        }


        // Adds action to display password widget (check box)
        _passwordProtectLinkAction = _linkContextMenu->addAction(tr("Password Protect"));
        _passwordProtectLinkAction->setCheckable(true);

        if(_linkShare->isPasswordSet()){
            _passwordProtectLinkAction->setChecked(true);
            _ui->lineEdit_password->setPlaceholderText("********");
            _ui->passwordShareProperty->show();
        }

        // If password is enforced then don't allow users to disable it
        if (_account->capabilities().sharePublicLinkEnforcePassword()) {
            _passwordProtectLinkAction->setChecked(true);
            _passwordProtectLinkAction->setEnabled(false);
            _passwordRequired = true;
        }

        // Adds action to display expiration date widget (check box)
        _expirationDateLinkAction = _linkContextMenu->addAction(tr("Expiration Date"));
        _expirationDateLinkAction->setCheckable(true);
        if(_linkShare->getExpireDate().isValid()){
            _expirationDateLinkAction->setChecked(true);
            _ui->expirationShareProperty->show();
        }


        // If expiredate is enforced do not allow disable and set max days
        if (_account->capabilities().sharePublicLinkEnforceExpireDate()) {
            _ui->calendar->setMaximumDate(QDate::currentDate().addDays(
                _account->capabilities().sharePublicLinkExpireDateDays()));
            _expirationDateLinkAction->setChecked(true);
            _expirationDateLinkAction->setEnabled(false);
            _expiryRequired = true;
        }

        // Adds action to unshare widget (check box)
        _unshareLinkAction = _linkContextMenu->addAction(QIcon(":/client/resources/delete.png"),
                                                         tr("Unshare"));

        connect(_linkContextMenu, &QMenu::triggered,
            this, &ShareLinkWidget::slotLinkContextMenuActionTriggered);

        _ui->shareLinkToolButton->setMenu(_linkContextMenu);
        _ui->shareLinkToolButton->setEnabled(true);
        _ui->enableShareLink->setEnabled(true);
        _ui->enableShareLink->setChecked(true);
    }
}

// TODO
//void ShareLinkWidget::slotShareSelectionChanged()
//{
//    // Disable running progress indicators
//    _pi_create->stopAnimation();
//    _pi_editing->stopAnimation();
//    _pi_date->stopAnimation();
//    _pi_password->stopAnimation();

//    _ui->errorLabel->hide();
//    _ui->passwordShareProperty->show();
//    _ui->expirationShareProperty->show();

//    if (!_account->capabilities().sharePublicLinkAllowUpload()) {
//        _allowUploadEditingLinkAction->setEnabled(false);
//        _allowUploadLinkAction->setEnabled(false);
//    }

//    // Password state
//    _ui->lineEdit_password->setEnabled(_linkShare->isPasswordSet());
//    if(_linkShare->isPasswordSet()) _ui->lineEdit_password->setPlaceholderText("********");
//    _ui->lineEdit_password->setText(QString());
//    _ui->lineEdit_password->setEnabled(_linkShare->isPasswordSet());
//    _ui->confirmPassword->setEnabled(_linkShare->isPasswordSet());

//    // Expiry state
//    _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));
//    if (_linkShare->getExpireDate().isValid()) {
//        _ui->calendar->setDate(_linkShare->getExpireDate());
//        _ui->calendar->setEnabled(true);
//    }
//    // Public upload state (box is hidden for files)
//    if (!_isFile) {
//        if (_linkShare->getPublicUpload()) {
//            if (_linkShare->getShowFileListing()) {
//                _allowUploadEditingLinkAction->setChecked(true);
//            } else {
//                _allowUploadLinkAction->setChecked(true);
//            }
//        } else {
//            _readOnlyLinkAction->setChecked(true);
//        }
//    }
//}

void ShareLinkWidget::setExpireDate(const QDate &date)
{
    if (_linkShare) {
        _pi_date->startAnimation();
        _ui->errorLabel->hide();
        _linkShare->setExpireDate(date);
    }
}

// TODO
//void ShareLinkWidget::slotExpireSet()
//{
//    if (sender() == _linkShare.data()) {
//        slotShareSelectionChanged();
//    }
//}

void ShareLinkWidget::slotExpireDateChanged(const QDate &date)
{
    setExpireDate(date);
}

void ShareLinkWidget::slotCreatePassword()
{
    if (!_manager) {
        return;
    }
    if (!_linkShare) {
        // If share creation requires a password, we'll be in this case
        if (_ui->lineEdit_password->text().isEmpty()) {
            _ui->lineEdit_password->setFocus();
            return;
        }

        _pi_create->startAnimation();
        _manager->createLinkShare(_sharePath, QString(), _ui->lineEdit_password->text());
    } else {
        setPassword(_ui->lineEdit_password->text());
    }
}

void ShareLinkWidget::slotCreateorDeleteShareLink(bool checked)
{
    if (!_manager) {
        qCWarning(lcSharing) << "No share manager set.";
        return;
    }

    _pi_create->startAnimation();
    if(checked){
        _manager->createLinkShare(_sharePath, QString(), QString());
    } else {
        if (!_linkShare) {
            qCWarning(lcSharing) << "No public link set.";
            return;
        }
        confirmAndDeleteShare();
    }

    _ui->shareLinkToolButton->setEnabled(checked);
}

void ShareLinkWidget::setPassword(const QString &password)
{
    if (_linkShare) {
        _pi_password->startAnimation();
        _ui->errorLabel->hide();
        _linkShare->setPassword(password);
    }
}

void ShareLinkWidget::slotPasswordSet()
{
    if (!_linkShare)
        return;

    _pi_password->stopAnimation();
    _ui->lineEdit_password->setText(QString());
    if (_linkShare->isPasswordSet()) {
        _ui->lineEdit_password->setPlaceholderText("********");
        _ui->lineEdit_password->setEnabled(true);
    } else {
        _ui->lineEdit_password->setPlaceholderText(QString());
    }

    /*
     * When setting/deleting a password from a share the old share is
     * deleted and a new one is created. So we need to refetch the shares
     * at this point.
     *
     * NOTE: I don't see this happening with oC > 10
     */
    getShares();
}

void ShareLinkWidget::slotDeleteShareFetched()
{
    getShares();
}

void ShareLinkWidget::slotCreateShareFetched()
{
    _pi_create->stopAnimation();
    _pi_password->stopAnimation();
    getShares();
}

void ShareLinkWidget::slotCreateShareRequiresPassword(const QString &message)
{
    // Prepare password entry
    _pi_create->stopAnimation();
    _pi_password->stopAnimation();
    _ui->passwordShareProperty->show();
    if (!message.isEmpty()) {
        _ui->errorLabel->setText(message);
        _ui->errorLabel->show();
    }

    _passwordRequired = true;

    togglePasswordOptions(true);
}

void ShareLinkWidget::togglePasswordOptions(bool enable)
{
    _ui->passwordShareProperty->setVisible(enable);
    if(enable) _ui->lineEdit_password->setFocus();
}

void ShareLinkWidget::toggleExpireDateOptions(bool enable)
{
    _ui->expirationShareProperty->setVisible(enable);
    if (enable) {
        const QDate date = QDate::currentDate().addDays(1);
        setExpireDate(date);
        _ui->calendar->setDate(date);
        _ui->calendar->setMinimumDate(date);
    }
}

void ShareLinkWidget::confirmAndDeleteShare()
{
    auto messageBox = new QMessageBox(
        QMessageBox::Question,
        tr("Confirm Link Share Deletion"),
        tr("<p>Do you really want to delete the public link share <i>%1</i>?</p>"
           "<p>Note: This action cannot be undone.</p>")
            .arg(shareName()),
        QMessageBox::NoButton,
        this);
    QPushButton *yesButton =
        messageBox->addButton(tr("Delete"), QMessageBox::YesRole);
    messageBox->addButton(tr("Cancel"), QMessageBox::NoRole);

    connect(messageBox, &QMessageBox::finished, this,
        [messageBox, yesButton, this]() {
        if (messageBox->clickedButton() == yesButton){
            // TODO: dlete is not hapenning correctly
            this->_linkShare->deleteShare();
            this->_ui->enableShareLink->setChecked(false);
            this->_ui->shareLinkToolButton->setEnabled(false);
        }
    });
    messageBox->open();
}

QString ShareLinkWidget::shareName() const
{
    QString name = _linkShare->getName();
    if (!name.isEmpty())
        return name;
    if (!_namesSupported)
        return tr("Public link");
    return _linkShare->getToken();
}

void ShareLinkWidget::slotContextMenuButtonClicked()
{
    _linkContextMenu->exec(QCursor::pos());
}

void ShareLinkWidget::slotLinkContextMenuActionTriggered(QAction *action)
{

    bool state = action->isChecked();
    SharePermissions perm = SharePermissionRead;

    if (action == _copyLinkAction) {
            QApplication::clipboard()->setText(_linkShare->getLink().toString());

    } else if (action == _readOnlyLinkAction && state) {
        _linkShare->setPermissions(perm);

    } else if (action == _allowEditingLinkAction && state) {
        perm |= SharePermissionUpdate;
        _linkShare->setPermissions(perm);

    } else if (action == _allowUploadEditingLinkAction && state) {
        perm |= SharePermissionCreate | SharePermissionUpdate | SharePermissionDelete;
        _linkShare->setPermissions(perm);

    } else if (action == _allowUploadLinkAction && state) {
        perm = SharePermissionCreate;
        _linkShare->setPermissions(perm);

    } else if (action == _passwordProtectLinkAction) {
        togglePasswordOptions(state);

    } else if (action == _expirationDateLinkAction) {
        toggleExpireDateOptions(state);

    } else if (action == _unshareLinkAction) {
        confirmAndDeleteShare();
    }
}

void ShareLinkWidget::slotServerError(int code, const QString &message)
{
    _pi_create->stopAnimation();
    _pi_date->stopAnimation();
    _pi_password->stopAnimation();
    _pi_editing->stopAnimation();

    qCWarning(lcSharing) << "Error from server" << code << message;
    displayError(message);
}

void ShareLinkWidget::slotPasswordSetError(int code, const QString &message)
{
    slotServerError(code, message);
    _ui->lineEdit_password->setFocus();
}

void ShareLinkWidget::displayError(const QString &errMsg)
{
    _ui->errorLabel->setText(errMsg);
    _ui->errorLabel->show();
}
}
