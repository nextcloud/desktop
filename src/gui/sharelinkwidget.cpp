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

const char propertyShareC[] = "oc_share";

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
    , _expiryRequired(false)
    , _namesSupported(true)
{
    _ui->setupUi(this);

    _ui->linkShares->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _ui->linkShares->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _ui->linkShares->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    //Is this a file or folder?
    QFileInfo fi(localPath);
    _isFile = fi.isFile();

    // Note: the share name cannot be longer than 64 characters
    _ui->nameLineEdit->setText(tr("Public link"));

    // the following progress indicator widgets are added to layouts which makes them
    // automatically deleted once the dialog dies.
    _pi_create = new QProgressIndicator();
    _pi_password = new QProgressIndicator();
    _pi_date = new QProgressIndicator();
    _pi_editing = new QProgressIndicator();
    _ui->horizontalLayout_create->insertWidget(2, _pi_create);
    _ui->horizontalLayout_password->addWidget(_pi_password);
    _ui->layout_editing->addWidget(_pi_editing);
    _ui->horizontalLayout_expire->insertWidget(_ui->horizontalLayout_expire->count() - 1, _pi_date);

    connect(_ui->nameLineEdit, &QLineEdit::returnPressed, this, &ShareLinkWidget::slotShareNameEntered);
    connect(_ui->createShareButton, &QAbstractButton::clicked, this, &ShareLinkWidget::slotShareNameEntered);
    connect(_ui->linkShares, &QTableWidget::itemSelectionChanged, this, &ShareLinkWidget::slotShareSelectionChanged);
    connect(_ui->linkShares, &QTableWidget::itemChanged, this, &ShareLinkWidget::slotNameEdited);
    connect(_ui->checkBox_password, &QAbstractButton::clicked, this, &ShareLinkWidget::slotCheckBoxPasswordClicked);
    connect(_ui->lineEdit_password, &QLineEdit::returnPressed, this, &ShareLinkWidget::slotPasswordReturnPressed);
    connect(_ui->lineEdit_password, &QLineEdit::textChanged, this, &ShareLinkWidget::slotPasswordChanged);
    connect(_ui->pushButton_setPassword, &QAbstractButton::clicked, this, &ShareLinkWidget::slotPasswordReturnPressed);
    connect(_ui->checkBox_expire, &QAbstractButton::clicked, this, &ShareLinkWidget::slotCheckBoxExpireClicked);
    connect(_ui->calendar, &QDateTimeEdit::dateChanged, this, &ShareLinkWidget::slotExpireDateChanged);
    connect(_ui->radio_readOnly, &QAbstractButton::clicked, this, &ShareLinkWidget::slotPermissionsClicked);
    connect(_ui->radio_readWrite, &QAbstractButton::clicked, this, &ShareLinkWidget::slotPermissionsClicked);
    connect(_ui->radio_uploadOnly, &QAbstractButton::clicked, this, &ShareLinkWidget::slotPermissionsClicked);

    _ui->errorLabel->hide();

    bool sharingPossible = true;
    if (!_account->capabilities().sharePublicLink()) {
        displayError(tr("Link shares have been disabled"));
        sharingPossible = false;
    } else if (!(maxSharingPermissions & SharePermissionShare)) {
        displayError(tr("The file can not be shared because it was shared without sharing permission."));
        sharingPossible = false;
    }
    if (!sharingPossible) {
        _ui->nameLineEdit->setEnabled(false);
        _ui->createShareButton->setEnabled(false);
    }

    // Older servers don't support multiple public link shares
    if (!_account->capabilities().sharePublicLinkMultiple()) {
        _namesSupported = false;
        _ui->createShareButton->setText(tr("Create public link share"));
        _ui->nameLineEdit->hide();
        _ui->nameLineEdit->clear(); // so we don't send a name
        _ui->nameLabel->hide();
    }

    _ui->shareProperties->setEnabled(false);

    _ui->pushButton_setPassword->setEnabled(false);
    _ui->lineEdit_password->setEnabled(false);
    _ui->pushButton_setPassword->setEnabled(false);
    _ui->checkBox_password->setText(tr("P&assword protect"));

    _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));
    _ui->calendar->setEnabled(false);

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


    // Parse capabilities

    if (!_account->capabilities().sharePublicLinkAllowUpload()) {
        _ui->radio_readWrite->setEnabled(false);
        _ui->radio_uploadOnly->setEnabled(false);
    }

    // If expiredate is enforced do not allow disable and set max days
    if (_account->capabilities().sharePublicLinkEnforceExpireDate()) {
        _ui->checkBox_expire->setEnabled(false);
        _ui->calendar->setMaximumDate(capabilityDefaultExpireDate());
        _expiryRequired = true;
    }

    // Hide permissions that are unavailable for files or disabled by capability.
    bool rwVisible = !_isFile && _account->capabilities().sharePublicLinkAllowUpload();
    bool uploadOnlyVisible = rwVisible && _account->capabilities().sharePublicLinkSupportsUploadOnly();
    _ui->radio_readWrite->setVisible(rwVisible);
    _ui->label_readWrite->setVisible(rwVisible);
    _ui->radio_uploadOnly->setVisible(uploadOnlyVisible);
    _ui->label_uploadOnly->setVisible(uploadOnlyVisible);

    // Prepare sharing menu

    _linkContextMenu = new QMenu(this);
    connect(_linkContextMenu, &QMenu::triggered,
        this, &ShareLinkWidget::slotLinkContextMenuActionTriggered);
    _openLinkAction = _linkContextMenu->addAction(tr("Open link in browser"));
    _copyLinkAction = _linkContextMenu->addAction(tr("Copy link to clipboard"));
    _copyDirectLinkAction = _linkContextMenu->addAction(tr("Copy link to clipboard (direct download)"));
    _emailLinkAction = _linkContextMenu->addAction(tr("Send link by email"));
    _emailDirectLinkAction = _linkContextMenu->addAction(tr("Send link by email (direct download)"));
    _deleteLinkAction = _linkContextMenu->addAction(tr("Delete"));

    /*
     * Create the share manager and connect it properly
     */
    if (sharingPossible) {
        _manager = new ShareManager(_account, this);
        connect(_manager, &ShareManager::sharesFetched, this, &ShareLinkWidget::slotSharesFetched);
        connect(_manager, &ShareManager::linkShareCreated, this, &ShareLinkWidget::slotCreateShareFetched);
        connect(_manager, &ShareManager::linkShareCreationForbidden, this, &ShareLinkWidget::slotCreateShareForbidden);
        connect(_manager, &ShareManager::serverError, this, &ShareLinkWidget::slotServerError);
    }

    auto retainSizeWhenHidden = [](QWidget *w) {
        auto sp = w->sizePolicy();
        sp.setRetainSizeWhenHidden(true);
        w->setSizePolicy(sp);
    };
    retainSizeWhenHidden(_ui->pushButton_setPassword);
    retainSizeWhenHidden(_ui->create);

    // If this starts out empty the first call to slotShareSelectionChanged()
    // will not properly initialize the ui state if "Create new..." is the
    // only option. So pre-fill with an invalid share id.
    _selectedShareId = QStringLiteral("!&no-share-selected");
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
    qCInfo(lcSharing) << "Fetched" << shares.count() << "shares";

    // Select the share that was previously selected,
    // except if an explicit override was asked for
    QString reselectShareId = _selectedShareId;
    if (!_newShareOverrideSelectionId.isEmpty()) {
        reselectShareId = _newShareOverrideSelectionId;
        _newShareOverrideSelectionId.clear();
    }

    auto table = _ui->linkShares;

    // Wipe the table without updating the ui elements, we
    // might want their state untouched if the same share ends
    // up being selected
    disconnect(table, &QTableWidget::itemSelectionChanged, this, &ShareLinkWidget::slotShareSelectionChanged);
    table->clearContents();
    table->setRowCount(0);
    connect(table, &QTableWidget::itemSelectionChanged, this, &ShareLinkWidget::slotShareSelectionChanged);

    auto deleteIcon = QIcon::fromTheme(QStringLiteral("user-trash"),
        Utility::getCoreIcon(QStringLiteral("delete")));

    for (const auto &share : shares) {
        if (share->getShareType() != Share::TypeLink) {
            continue;
        }
        auto linkShare = qSharedPointerDynamicCast<LinkShare>(share);

        // Connect all shares signals to gui slots
        connect(share.data(), &Share::serverError, this, &ShareLinkWidget::slotServerError);
        connect(share.data(), &Share::shareDeleted, this, &ShareLinkWidget::slotDeleteShareFetched);
        connect(share.data(), &Share::permissionsSet, this, &ShareLinkWidget::slotPermissionsSet);
        connect(linkShare.data(), &LinkShare::expireDateSet, this, &ShareLinkWidget::slotExpireSet);
        connect(linkShare.data(), &LinkShare::passwordSet, this, &ShareLinkWidget::slotPasswordSet);
        connect(linkShare.data(), &LinkShare::passwordSetError, this, &ShareLinkWidget::slotPasswordSetError);

        // Build the table row
        auto row = table->rowCount();
        table->insertRow(row);

        auto nameItem = new QTableWidgetItem;
        auto name = shareName(*linkShare);
        if (!_namesSupported) {
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        }
        nameItem->setText(name);
        nameItem->setData(Qt::UserRole, QVariant::fromValue(linkShare));
        table->setItem(row, 0, nameItem);

        auto dotdotdotButton = new QToolButton;
        dotdotdotButton->setText(QStringLiteral("..."));
        dotdotdotButton->setProperty(propertyShareC, QVariant::fromValue(linkShare));
        connect(dotdotdotButton, &QAbstractButton::clicked, this, &ShareLinkWidget::slotContextMenuButtonClicked);
        table->setCellWidget(row, 1, dotdotdotButton);

        auto deleteButton = new QToolButton;
        deleteButton->setIcon(deleteIcon);
        deleteButton->setProperty(propertyShareC, QVariant::fromValue(linkShare));
        deleteButton->setToolTip(tr("Delete link share"));
        connect(deleteButton, &QAbstractButton::clicked, this, &ShareLinkWidget::slotDeleteShareClicked);
        table->setCellWidget(row, 2, deleteButton);

        // Reestablish the previous selection
        if (reselectShareId == share->getId()) {
            table->selectRow(row);
        }
    }

    // Add create-new entry
    if (_namesSupported || table->rowCount() == 0) {
        auto row = table->rowCount();
        table->insertRow(row);
        auto createItem = new QTableWidgetItem;
        createItem->setFlags(createItem->flags() & ~Qt::ItemIsEditable);
        createItem->setText(tr("Create new..."));
        auto font = createItem->font();
        font.setItalic(true);
        createItem->setFont(font);
        table->setItem(row, 0, createItem);
        auto dummyItem = new QTableWidgetItem;
        dummyItem->setFlags(dummyItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 1, dummyItem->clone());
        table->setItem(row, 2, dummyItem);
    }

    if (!selectedShare()) {
        if (table->rowCount() != 0) {
            // Select the first share by default
            table->selectRow(0);
        } else {
            // explicitly note the deselection,
            // since this was not triggered on table clear above
            slotShareSelectionChanged();
        }
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
    bool selectionUnchanged = false;
    bool createNew = !share;
    if (share) {
        selectionUnchanged = _selectedShareId == share->getId();
        _selectedShareId = share->getId();
    } else {
        selectionUnchanged = _selectedShareId.isEmpty();
        _selectedShareId.clear();
    }

    _ui->shareProperties->setEnabled(true);

    // Public upload state (files can only be read-only, box is hidden for them)
    _ui->widget_editing->setEnabled(!_isFile);
    if (!selectionUnchanged) {
        if (share && share->getPublicUpload()) {
            if (share->getShowFileListing()) {
                _ui->radio_readWrite->setChecked(true);
            } else {
                _ui->radio_uploadOnly->setChecked(true);
            }
        } else {
            _ui->radio_readOnly->setChecked(true);
        }
    }

    const auto capabilities = _account->capabilities();
    const bool passwordRequired =
        (_ui->radio_readOnly->isChecked() && capabilities.sharePublicLinkEnforcePasswordForReadOnly())
        || (_ui->radio_readWrite->isChecked() && capabilities.sharePublicLinkEnforcePasswordForReadWrite())
        || (_ui->radio_uploadOnly->isChecked() && capabilities.sharePublicLinkEnforcePasswordForUploadOnly());

    // Password state
    _ui->pushButton_setPassword->setVisible(!createNew);
    _ui->checkBox_password->setText(tr("P&assword protect"));
    if (createNew) {
        if (passwordRequired) {
            _ui->checkBox_password->setChecked(true);
            _ui->lineEdit_password->setPlaceholderText(tr("Please Set Password"));
            _ui->lineEdit_password->setEnabled(true);
        } else if (!selectionUnchanged || _ui->lineEdit_password->text().isEmpty()) {
            // force to off if no password was entered yet
            _ui->checkBox_password->setChecked(false);
            _ui->lineEdit_password->setPlaceholderText(QString());
            _ui->lineEdit_password->setEnabled(false);
        }
    } else if (!selectionUnchanged) {
        // we used to disable the checkbox (and thus the line edit) when setting a password failed
        // now, we leave it enabled but clear the password in any case
        // only if setting was successful, we want to show a placeholder afterwards to signalize success
        if (share && share->isPasswordSet()) {
            _ui->checkBox_password->setChecked(true);
            _ui->lineEdit_password->setPlaceholderText(QStringLiteral("********"));
            _ui->lineEdit_password->setEnabled(true);
        }
        _ui->lineEdit_password->setText(QString());
        _ui->pushButton_setPassword->setEnabled(false);
    }
    // The following is done to work with old shares when the pw requirement
    // is enabled on the server later: users can add pws to old shares.
    _ui->checkBox_password->setEnabled(!_ui->checkBox_password->isChecked() || !passwordRequired);

    // Expiry state
    _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));
    if (!selectionUnchanged) {
        if (share && share->getExpireDate().isValid()) {
            _ui->checkBox_expire->setChecked(true);
            _ui->calendar->setDate(share->getExpireDate());
            _ui->calendar->setEnabled(true);
        } else if (createNew) {
            const QDate defaultExpire = capabilityDefaultExpireDate();
            if (defaultExpire.isValid())
                _ui->calendar->setDate(defaultExpire);
            const bool enabled = _expiryRequired || defaultExpire.isValid();
            _ui->checkBox_expire->setChecked(enabled);
            _ui->calendar->setEnabled(enabled);
        } else {
            _ui->checkBox_expire->setChecked(false);
            _ui->calendar->setEnabled(false);
        }
    }
    // Allows checking expiry on old shares created before it was required.
    _ui->checkBox_expire->setEnabled(!_ui->checkBox_expire->isChecked() || !_expiryRequired);

    // Name and create button
    _ui->create->setVisible(createNew);
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
    if (selectedShare()) {
        setPassword(_ui->lineEdit_password->text());
        _ui->lineEdit_password->clearFocus();
    }
}

void ShareLinkWidget::slotPasswordChanged(const QString &newText)
{
    // disable the set-password button
    _ui->pushButton_setPassword->setEnabled(newText.length() > 0);
}

void ShareLinkWidget::slotNameEdited(QTableWidgetItem *item)
{
    if (!_namesSupported) {
        return;
    }

    QString newName = item->text();
    auto share = item->data(Qt::UserRole).value<QSharedPointer<LinkShare>>();
    if (share && newName != share->getName() && newName != share->getToken()) {
        share->setName(newName);

        // the server doesn't necessarily apply the desired name but may assign a different one
        // for instance, when the user removes a custom name by emptying the field, the server assigns the ID as name
        // therefore, we need to fetch the name(s) from the server again to display accurate information
        getShares();
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
    auto share = selectedShare();
    if (sender() != share.data())
        return;

    _pi_password->stopAnimation();
    _ui->checkBox_password->setEnabled(true);
    _ui->lineEdit_password->setText(QString());
    if (share->isPasswordSet()) {
        _ui->lineEdit_password->setPlaceholderText(QStringLiteral("********"));
        _ui->lineEdit_password->setEnabled(true);
    } else {
        _ui->lineEdit_password->setPlaceholderText(QString());
        _ui->lineEdit_password->setEnabled(false);
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

void ShareLinkWidget::slotShareNameEntered()
{
    if (!_manager) {
        return;
    }
    _pi_create->startAnimation();
    _manager->createLinkShare(
            _sharePath,
            _ui->nameLineEdit->text(),
            _ui->checkBox_password->isChecked() ? _ui->lineEdit_password->text() : QString(),
            _ui->checkBox_expire->isChecked() ? _ui->calendar->date() : QDate(),
            uiPermissionState());
}

void ShareLinkWidget::slotDeleteShareFetched()
{
    getShares();
}

void ShareLinkWidget::slotCreateShareFetched(const QSharedPointer<LinkShare> &share)
{
    _pi_create->stopAnimation();
    _pi_password->stopAnimation();
    _ui->nameLineEdit->clear();

    _newShareOverrideSelectionId = share->getId();
    getShares();
}

void ShareLinkWidget::slotCreateShareForbidden(const QString &message)
{
    // Show the message, so users can adjust and try again
    _pi_create->stopAnimation();
    if (!message.isEmpty()) {
        _ui->errorLabel->setText(message);
        _ui->errorLabel->show();
    }
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
    if (_ui->checkBox_expire->checkState() == Qt::Checked) {
        const QDate tomorrow = QDate::currentDate().addDays(1);
        QDate defaultDate = capabilityDefaultExpireDate();
        if (!defaultDate.isValid())
            defaultDate = tomorrow;
        setExpireDate(defaultDate);
        _ui->calendar->setDate(defaultDate);
        _ui->calendar->setMinimumDate(tomorrow);
        _ui->calendar->setEnabled(true);
    } else {
        setExpireDate(QDate());
        _ui->calendar->setEnabled(false);
    }
}

void ShareLinkWidget::emailShareLink(const QUrl &url)
{
    QString fileName = _sharePath.mid(_sharePath.lastIndexOf('/') + 1);
    Utility::openEmailComposer(
        tr("I shared %1 with you").arg(fileName),
        url.toString(),
        this);
}

void ShareLinkWidget::openShareLink(const QUrl &url)
{
    Utility::openBrowser(url, this);
}

void ShareLinkWidget::confirmAndDeleteShare(const QSharedPointer<LinkShare> &share)
{
    auto messageBox = new QMessageBox(
        QMessageBox::Question,
        tr("Confirm Link Share Deletion"),
        tr("<p>Do you really want to delete the public link share <i>%1</i>?</p>"
           "<p>Note: This action cannot be undone.</p>")
            .arg(shareName(*share)),
        QMessageBox::NoButton,
        this);
    QPushButton *yesButton =
        messageBox->addButton(tr("Delete"), QMessageBox::YesRole);
    messageBox->addButton(tr("Cancel"), QMessageBox::NoRole);

    connect(messageBox, &QMessageBox::finished, this,
        [messageBox, yesButton, share]() {
        if (messageBox->clickedButton() == yesButton)
            share->deleteShare();
    });
    messageBox->open();
}

QString ShareLinkWidget::shareName(const LinkShare &share) const
{
    QString name = share.getName();
    if (!name.isEmpty())
        return name;
    if (!_namesSupported)
        return tr("Public link");
    return share.getToken();
}

void ShareLinkWidget::slotContextMenuButtonClicked()
{
    auto share = sender()->property(propertyShareC).value<QSharedPointer<LinkShare>>();
    bool downloadEnabled = share->getShowFileListing();
    _copyDirectLinkAction->setVisible(downloadEnabled);
    _emailDirectLinkAction->setVisible(downloadEnabled);

    _linkContextMenu->setProperty(propertyShareC, QVariant::fromValue(share));
    _linkContextMenu->exec(QCursor::pos());
}

void ShareLinkWidget::slotLinkContextMenuActionTriggered(QAction *action)
{
    auto share = sender()->property(propertyShareC).value<QSharedPointer<LinkShare>>();

    if (action == _deleteLinkAction) {
        confirmAndDeleteShare(share);
    } else if (action == _copyLinkAction) {
        QApplication::clipboard()->setText(share->getLink().toString());
    } else if (action == _copyDirectLinkAction) {
        QApplication::clipboard()->setText(share->getDirectDownloadLink().toString());
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
    confirmAndDeleteShare(share);
}

SharePermissions ShareLinkWidget::uiPermissionState() const
{
    if (_ui->radio_readWrite->isChecked()) {
        return SharePermissionRead | SharePermissionCreate
            | SharePermissionUpdate | SharePermissionDelete;
    } else if (_ui->radio_uploadOnly->isChecked()) {
        return SharePermissionCreate;
    }
    return SharePermissionRead;
}

void ShareLinkWidget::slotPermissionsClicked()
{
    if (auto current = selectedShare()) {
        _ui->widget_editing->setEnabled(false);
        _pi_editing->startAnimation();
        _ui->errorLabel->hide();

        current->setPermissions(uiPermissionState());
    } else {
        // Password may now be required, update ui.
        slotShareSelectionChanged();
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

QDate ShareLinkWidget::capabilityDefaultExpireDate() const
{
    if (!_account->capabilities().sharePublicLinkDefaultExpire())
        return QDate();
    return QDate::currentDate().addDays(
            _account->capabilities().sharePublicLinkDefaultExpireDateDays());
}

void ShareLinkWidget::slotPermissionsSet()
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

    // Reset UI state
    _selectedShareId.clear();
    slotShareSelectionChanged();

    qCWarning(lcSharing) << "Error from server" << code << message;
    displayError(message);
}

void ShareLinkWidget::slotPasswordSetError(int code, const QString &message)
{
    slotServerError(code, message);

    _ui->lineEdit_password->setEnabled(true);
    _ui->lineEdit_password->setFocus();
}

void ShareLinkWidget::displayError(const QString &errMsg)
{
    _ui->errorLabel->setText(errMsg);
    _ui->errorLabel->show();
}
}
