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

#include "QProgressIndicator.h"
#include <QBuffer>
#include <QClipboard>
#include <QFileInfo>
#include <QDesktopServices>
#include <QMessageBox>
#include <QMenu>
#include <QToolButton>

namespace OCC {

const char propertyShareC[] = "oc_share";

ShareLinkWidget::ShareLinkWidget(AccountPtr account,
                                 const QString &sharePath,
                                 const QString &localPath,
                                 SharePermissions maxSharingPermissions,
                                 QWidget *parent) :
   QWidget(parent),
    _ui(new Ui::ShareLinkWidget),
    _account(account),
    _sharePath(sharePath),
    _localPath(localPath),
    _manager(0),
    _passwordRequired(false),
    _expiryRequired(false),
    _namesSupported(true)
{
    _ui->setupUi(this);

    _ui->linkShares->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _ui->linkShares->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _ui->linkShares->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    //Is this a file or folder?
    QFileInfo fi(localPath);
    _isFile = fi.isFile();
    _ui->nameLineEdit->setText(tr("%1 link").arg(fi.fileName()));

    // the following progress indicator widgets are added to layouts which makes them
    // automatically deleted once the dialog dies.
    _pi_create   = new QProgressIndicator();
    _pi_password = new QProgressIndicator();
    _pi_date     = new QProgressIndicator();
    _pi_editing  = new QProgressIndicator();
    _ui->horizontalLayout_create->addWidget(_pi_create);
    _ui->horizontalLayout_password->addWidget(_pi_password);
    _ui->horizontalLayout_editing->addWidget(_pi_editing);
    _ui->horizontalLayout_expire->insertWidget(_ui->horizontalLayout_expire->count() - 1, _pi_date);

    connect(_ui->nameLineEdit, SIGNAL(returnPressed()), SLOT(slotShareNameEntered()));
    connect(_ui->createShareButton, SIGNAL(clicked(bool)), SLOT(slotShareNameEntered()));
    connect(_ui->linkShares, SIGNAL(itemSelectionChanged()), SLOT(slotShareSelectionChanged()));
    connect(_ui->linkShares, SIGNAL(itemChanged(QTableWidgetItem*)), SLOT(slotNameEdited(QTableWidgetItem*)));
    connect(_ui->checkBox_password, SIGNAL(clicked()), this, SLOT(slotCheckBoxPasswordClicked()));
    connect(_ui->lineEdit_password, SIGNAL(returnPressed()), this, SLOT(slotPasswordReturnPressed()));
    connect(_ui->lineEdit_password, SIGNAL(textChanged(QString)), this, SLOT(slotPasswordChanged(QString)));
    connect(_ui->pushButton_setPassword, SIGNAL(clicked(bool)), SLOT(slotPasswordReturnPressed()));
    connect(_ui->checkBox_expire, SIGNAL(clicked()), this, SLOT(slotCheckBoxExpireClicked()));
    connect(_ui->calendar, SIGNAL(dateChanged(QDate)), SLOT(slotExpireDateChanged(QDate)));
    connect(_ui->checkBox_editing, SIGNAL(clicked()), this, SLOT(slotCheckBoxEditingClicked()));

    if (!_account->capabilities().sharePublicLink()) {
        displayError(tr("Link shares have been disabled"));
        _ui->nameLineEdit->setEnabled(false);
        _ui->createShareButton->setEnabled(false);
    } else if ( !(maxSharingPermissions & SharePermissionShare) ) {
        displayError(tr("The file can not be shared because it was shared without sharing permission."));
        _ui->nameLineEdit->setEnabled(false);
        _ui->createShareButton->setEnabled(false);
    }

    // Older servers don't support multiple public link shares
    if (!_account->capabilities().sharePublicLinkMultiple()) {
        _namesSupported = false;
        _ui->createShareButton->setText(tr("Create public link share"));
        _ui->nameLineEdit->hide();
        _ui->nameLineEdit->clear(); // so we don't send a name
    }

    _ui->shareProperties->setEnabled(false);

    _ui->pushButton_setPassword->setEnabled(false);
    _ui->lineEdit_password->setEnabled(false);
    _ui->pushButton_setPassword->setEnabled(false);
    _ui->checkBox_password->setText(tr("P&assword protect"));

    _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));
    _ui->calendar->setEnabled(false);

    // check if the file is already inside of a synced folder
    if( sharePath.isEmpty() ) {
        // The file is not yet in an ownCloud synced folder. We could automatically
        // copy it over, but that is skipped as not all questions can be answered that
        // are involved in that, see https://github.com/owncloud/client/issues/2732
        //
        // _ui->checkBox_shareLink->setEnabled(false);
        // uploadExternalFile();
        qCDebug(lcSharing) << "Unable to share files not in a sync folder.";
        return;
    }

    _ui->errorLabel->hide();


    // Parse capabilities

    // If password is enforced then don't allow users to disable it
    if (_account->capabilities().sharePublicLinkEnforcePassword()) {
        _ui->checkBox_password->setEnabled(false);
        _passwordRequired = true;
    }

    // If expiredate is enforced do not allow disable and set max days
    if (_account->capabilities().sharePublicLinkEnforceExpireDate()) {
        _ui->checkBox_expire->setEnabled(false);
        _ui->calendar->setMaximumDate(QDate::currentDate().addDays(
            _account->capabilities().sharePublicLinkExpireDateDays()
            ));
        _expiryRequired = true;
    }

    // File can't have public upload set.
    _ui->widget_editing->setVisible(!_isFile);
    _ui->checkBox_editing->setEnabled(
            _account->capabilities().sharePublicLinkAllowUpload());


    // Prepare sharing menu

    _shareLinkMenu = new QMenu(this);
    _openLinkAction = _shareLinkMenu->addAction(tr("Open link in browser"));
    _copyLinkAction = _shareLinkMenu->addAction(tr("Copy link to clipboard"));
    _copyDirectLinkAction = _shareLinkMenu->addAction(tr("Copy link to clipboard (direct download)"));
    _emailLinkAction = _shareLinkMenu->addAction(tr("Send link by email"));
    _emailDirectLinkAction = _shareLinkMenu->addAction(tr("Send link by email (direct download)"));

    /*
     * Create the share manager and connect it properly
     */
    _manager = new ShareManager(_account, this);

    connect(_manager, SIGNAL(sharesFetched(QList<QSharedPointer<Share>>)), SLOT(slotSharesFetched(QList<QSharedPointer<Share>>)));
    connect(_manager, SIGNAL(linkShareCreated(QSharedPointer<LinkShare>)), SLOT(slotCreateShareFetched(const QSharedPointer<LinkShare>)));
    connect(_manager, SIGNAL(linkShareRequiresPassword(QString)), SLOT(slotCreateShareRequiresPassword(QString)));
    connect(_manager, SIGNAL(serverError(int, QString)), SLOT(slotServerError(int,QString)));
}

ShareLinkWidget::~ShareLinkWidget()
{
    delete _ui;
}

void ShareLinkWidget::getShares()
{
    _manager->fetchShares(_sharePath);
}

void ShareLinkWidget::slotSharesFetched(const QList<QSharedPointer<Share>> &shares)
{
    const QString versionString = _account->serverVersion();
    qCDebug(lcSharing) << versionString << "Fetched" << shares.count() << "shares";

    // Preserve the previous selection
    QString selectedShareId;
    if (auto share = selectedShare()) {
        selectedShareId = share->getId();
    }
    // ...except if selection should move to a new share
    if (!_newShareOverrideSelectionId.isEmpty()) {
        selectedShareId = _newShareOverrideSelectionId;
        _newShareOverrideSelectionId.clear();
    }

    auto table = _ui->linkShares;
    table->clearContents();
    table->setRowCount(0);

    auto deleteIcon = QIcon::fromTheme(QLatin1String("user-trash"),
                                       QIcon(QLatin1String(":/client/resources/delete.png")));

    foreach (auto share, shares) {
        if (share->getShareType() != Share::TypeLink) {
            continue;
        }
        auto linkShare = qSharedPointerDynamicCast<LinkShare>(share);

        // Connect all shares signals to gui slots
        connect(share.data(), SIGNAL(serverError(int, QString)), SLOT(slotServerError(int,QString)));
        connect(share.data(), SIGNAL(shareDeleted()), SLOT(slotDeleteShareFetched()));
        connect(share.data(), SIGNAL(expireDateSet()), SLOT(slotExpireSet()));
        connect(share.data(), SIGNAL(publicUploadSet()), SLOT(slotPublicUploadSet()));
        connect(share.data(), SIGNAL(passwordSet()), SLOT(slotPasswordSet()));
        connect(share.data(), SIGNAL(passwordSetError(int, QString)), SLOT(slotPasswordSetError(int,QString)));

        // Build the table row
        auto row = table->rowCount();
        table->insertRow(row);

        auto nameItem = new QTableWidgetItem;
        QString name = linkShare->getName();
        if (name.isEmpty()) {
            if (!_namesSupported) {
                name = tr("Public link");
                nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            } else {
                name = linkShare->getToken();
            }
        }
        nameItem->setText(name);
        nameItem->setData(Qt::UserRole, QVariant::fromValue(linkShare));
        table->setItem(row, 0, nameItem);

        auto shareButton = new QToolButton;
        shareButton->setText("...");
        shareButton->setProperty(propertyShareC, QVariant::fromValue(linkShare));
        shareButton->setMenu(_shareLinkMenu);
        shareButton->setPopupMode(QToolButton::InstantPopup);
        connect(shareButton, SIGNAL(triggered(QAction*)), SLOT(slotShareLinkButtonTriggered(QAction*)));
        table->setCellWidget(row, 1, shareButton);

        auto deleteButton = new QToolButton;
        deleteButton->setIcon(deleteIcon);
        deleteButton->setProperty(propertyShareC, QVariant::fromValue(linkShare));
        connect(deleteButton, SIGNAL(clicked(bool)), SLOT(slotDeleteShareClicked()));
        table->setCellWidget(row, 2, deleteButton);

        // Reestablish the previous selection
        if (selectedShareId == share->getId()) {
            table->selectRow(row);
        }
    }

    // Select the first share by default
    if (!selectedShare() && table->rowCount() != 0) {
        table->selectRow(0);
    }

    if (!_namesSupported) {
        _ui->createShareButton->setEnabled(table->rowCount() == 0);
    }
}

void ShareLinkWidget::slotShareSelectionChanged()
{
    // Disable running progress indicators
    _pi_create->stopAnimation();
    _pi_editing->stopAnimation();
    _pi_date->stopAnimation();
    _pi_password->stopAnimation();

    _ui->errorLabel->hide();

    auto share = selectedShare();
    if (!share) {
        _ui->shareProperties->setEnabled(false);
        _ui->checkBox_editing->setChecked(false);
        _ui->checkBox_expire->setChecked(false);
        _ui->checkBox_password->setChecked(false);
        return;
    }

    _ui->shareProperties->setEnabled(true);

    _ui->checkBox_password->setEnabled(!_passwordRequired);
    _ui->checkBox_expire->setEnabled(!_expiryRequired);
    _ui->checkBox_editing->setEnabled(
            _account->capabilities().sharePublicLinkAllowUpload());

    // Password state
    _ui->checkBox_password->setText(tr("P&assword protect"));
    if (share->isPasswordSet()) {
        _ui->checkBox_password->setChecked(true);
        _ui->lineEdit_password->setEnabled(true);
        _ui->lineEdit_password->setPlaceholderText("********");
        _ui->lineEdit_password->setText(QString());
        _ui->lineEdit_password->setEnabled(true);
        _ui->pushButton_setPassword->setEnabled(true);
    } else {
        _ui->checkBox_password->setChecked(false);
        _ui->lineEdit_password->setPlaceholderText(QString());
        _ui->lineEdit_password->setEnabled(false);
        _ui->pushButton_setPassword->setEnabled(false);
    }

    // Expiry state
    _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));
    if (share->getExpireDate().isValid()) {
        _ui->checkBox_expire->setChecked(true);
        _ui->calendar->setDate(share->getExpireDate());
        _ui->calendar->setEnabled(true);
    } else {
        _ui->checkBox_expire->setChecked(false);
        _ui->calendar->setEnabled(false);
    }

    // Public upload state (box is hidden for files)
    if (!_isFile) {
        _ui->checkBox_editing->setChecked(share->getPublicUpload());
    }
}

void ShareLinkWidget::setExpireDate(const QDate &date)
{
    if (auto current = selectedShare()) {
        _pi_date->startAnimation();
        _ui->errorLabel->hide();
        current->setExpireDate(date);
    }
}

void ShareLinkWidget::slotExpireSet()
{
    if (sender() == selectedShare().data()) {
        slotShareSelectionChanged();
    }
}

void ShareLinkWidget::slotExpireDateChanged(const QDate &date)
{
    if (_ui->checkBox_expire->isChecked()) {
        setExpireDate(date);
    }
}

void ShareLinkWidget::slotPasswordReturnPressed()
{
    if (!selectedShare()) {
        // If share creation requires a password, we'll be in this case
        if (_ui->lineEdit_password->text().isEmpty()) {
            _ui->lineEdit_password->setFocus();
            return;
        }

        _pi_create->startAnimation();
        _manager->createLinkShare(_sharePath, _ui->nameLineEdit->text(), _ui->lineEdit_password->text());
    } else {
        setPassword(_ui->lineEdit_password->text());
    }
    _ui->lineEdit_password->clearFocus();
}

void ShareLinkWidget::slotPasswordChanged(const QString& newText)
{
    // disable the set-password button
    _ui->pushButton_setPassword->setEnabled( newText.length() > 0 );
}

void ShareLinkWidget::slotNameEdited(QTableWidgetItem* item)
{
    if (!_namesSupported) {
        return;
    }

    QString newName = item->text();
    auto share = item->data(Qt::UserRole).value<QSharedPointer<LinkShare>>();
    if (share && newName != share->getName() && newName != share->getToken()) {
        share->setName(newName);
    }
}

void ShareLinkWidget::setPassword(const QString &password)
{
    if (auto current = selectedShare()) {
        _pi_password->startAnimation();
        _ui->errorLabel->hide();

        _ui->checkBox_password->setEnabled(false);
        _ui->lineEdit_password->setEnabled(false);

        current->setPassword(password);
    }
}

void ShareLinkWidget::slotPasswordSet()
{
    _pi_password->stopAnimation();
    _ui->lineEdit_password->setText(QString());
    _ui->lineEdit_password->setPlaceholderText(tr("Password Protected"));

    /*
     * When setting/deleting a password from a share the old share is
     * deleted and a new one is created. So we need to refetch the shares
     * at this point.
     */
    getShares();
}

void ShareLinkWidget::slotShareNameEntered()
{
    _pi_create->startAnimation();
    _manager->createLinkShare(_sharePath, _ui->nameLineEdit->text(), QString());
}

void ShareLinkWidget::slotDeleteShareFetched()
{
    getShares();
}

void ShareLinkWidget::slotCreateShareFetched(const QSharedPointer<LinkShare> share)
{
    _pi_create->stopAnimation();
    _pi_password->stopAnimation();
    _ui->nameLineEdit->clear();

    _newShareOverrideSelectionId = share->getId();
    getShares();
}

void ShareLinkWidget::slotCreateShareRequiresPassword(const QString& message)
{
    // Deselect existing shares
    _ui->linkShares->clearSelection();

    // Prepare password entry
    _pi_create->stopAnimation();
    _pi_password->stopAnimation();
    _ui->shareProperties->setEnabled(true);
    _ui->checkBox_password->setChecked(true);
    _ui->checkBox_password->setEnabled(false);
    _ui->checkBox_password->setText(tr("Public sh&aring requires a password"));
    _ui->checkBox_expire->setEnabled(false);
    _ui->checkBox_editing->setEnabled(false);
    if (!message.isEmpty()) {
        _ui->errorLabel->setText(message);
        _ui->errorLabel->show();
    }

    _passwordRequired = true;

    slotCheckBoxPasswordClicked();
}

void ShareLinkWidget::slotCheckBoxPasswordClicked()
{
    if (_ui->checkBox_password->checkState() == Qt::Checked) {
        _ui->lineEdit_password->setEnabled(true);
        _ui->pushButton_setPassword->setEnabled(true);
        _ui->lineEdit_password->setPlaceholderText(tr("Please Set Password"));
        _ui->lineEdit_password->setFocus();
    } else {
        setPassword(QString());
        _ui->lineEdit_password->setPlaceholderText(QString());
        _pi_password->startAnimation();
        _ui->lineEdit_password->setEnabled(false);
        _ui->pushButton_setPassword->setEnabled(false);
    }
}

void ShareLinkWidget::slotCheckBoxExpireClicked()
{
    if (_ui->checkBox_expire->checkState() == Qt::Checked)
    {
        const QDate date = QDate::currentDate().addDays(1);
        setExpireDate(date);
        _ui->calendar->setDate(date);
        _ui->calendar->setMinimumDate(date);
        _ui->calendar->setEnabled(true);
    }
    else
    {
        setExpireDate(QDate());
        _ui->calendar->setEnabled(false);
    }
}

#ifdef Q_OS_MAC
extern void copyToPasteboard(const QString &string);
#endif

void ShareLinkWidget::copyShareLink(const QUrl &url)
{
#ifdef Q_OS_MAC
    copyToPasteboard(url.toString());
#else
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(url.toString());
#endif
}

void ShareLinkWidget::emailShareLink(const QUrl &url)
{
    QString fileName = _sharePath.mid(_sharePath.lastIndexOf('/') + 1);

    if (!QDesktopServices::openUrl(QUrl(QString(
                "mailto: "
                "?subject=I shared %1 with you"
                "&body=%2").arg(
                fileName,
                url.toString()),
            QUrl::TolerantMode))) {
        QMessageBox::warning(
            this,
            tr("Could not open email client"),
            tr("There was an error when launching the email client to "
               "create a new message. Maybe no default email client is "
               "configured?"));
    }
}

void ShareLinkWidget::openShareLink(const QUrl &url)
{
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::warning(
            this,
            tr("Could not open browser"),
            tr("There was an error when launching the browser to "
               "view the public link share. Maybe no default browser is "
               "configured?"));
    }
}

void ShareLinkWidget::slotShareLinkButtonTriggered(QAction *action)
{
    auto share = sender()->property(propertyShareC).value<QSharedPointer<LinkShare>>();

    if (action == _copyLinkAction) {
        copyShareLink(share->getLink());
    } else if (action == _copyDirectLinkAction) {
        copyShareLink(share->getDirectDownloadLink());
    } else if (action == _emailLinkAction) {
        emailShareLink(share->getLink());
    } else if (action == _emailDirectLinkAction) {
        emailShareLink(share->getDirectDownloadLink());
    } else if (action == _openLinkAction) {
        openShareLink(share->getLink());
    }
}

void ShareLinkWidget::slotDeleteShareClicked()
{
    auto share = sender()->property(propertyShareC).value<QSharedPointer<LinkShare>>();
    share->deleteShare();
}

void ShareLinkWidget::slotCheckBoxEditingClicked()
{
    if (auto current = selectedShare()) {
        _ui->checkBox_editing->setEnabled(false);
        _pi_editing->startAnimation();
        _ui->errorLabel->hide();

        current->setPublicUpload(_ui->checkBox_editing->isChecked());
    }
}

QSharedPointer<LinkShare> ShareLinkWidget::selectedShare() const
{
    const auto items = _ui->linkShares->selectedItems();
    if (items.isEmpty()) {
        return QSharedPointer<LinkShare>();
    }

    return items.first()->data(Qt::UserRole).value<QSharedPointer<LinkShare>>();
}

void ShareLinkWidget::slotPublicUploadSet()
{
    if (sender() == selectedShare().data()) {
        slotShareSelectionChanged();
    }
}

void ShareLinkWidget::slotServerError(int code, const QString &message)
{
    _pi_create->stopAnimation();
    _pi_date->stopAnimation();
    _pi_password->stopAnimation();
    _pi_editing->stopAnimation();

    qCDebug(lcSharing) << "Error from server" << code << message;
    displayError(message);
}

void ShareLinkWidget::slotPasswordSetError(int code, const QString &message)
{
    slotServerError(code, message);

    _ui->checkBox_password->setEnabled(!_passwordRequired);
    _ui->lineEdit_password->setEnabled(true);
    _ui->lineEdit_password->setFocus();
}

void ShareLinkWidget::displayError(const QString& errMsg)
{
    _ui->errorLabel->setText( errMsg );
    _ui->errorLabel->show();
}

}
