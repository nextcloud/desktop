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

#include "ui_sharelinkwidget.h"
#include "sharelinkwidget.h"
#include "account.h"
#include "capabilities.h"
#include "guiutility.h"
#include "sharemanager.h"

#include "QProgressIndicator.h"
#include <QBuffer>
#include <QClipboard>
#include <QFileInfo>
#include <QDesktopServices>
#include <QMessageBox>
#include <QMenu>
#include <QToolButton>
#include <QPropertyAnimation>

namespace OCC {

Q_LOGGING_CATEGORY(lcShareLink, "nextcloud.gui.sharelink", QtInfoMsg)

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
    , _linkShare(nullptr)
    , _passwordRequired(false)
    , _expiryRequired(false)
    , _namesSupported(true)
    , _linkContextMenu(nullptr)
    , _readOnlyLinkAction(nullptr)
    , _allowEditingLinkAction(nullptr)
    , _allowUploadEditingLinkAction(nullptr)
    , _allowUploadLinkAction(nullptr)
    , _passwordProtectLinkAction(nullptr)
    , _expirationDateLinkAction(nullptr)
    , _unshareLinkAction(nullptr)
{
    _ui->setupUi(this);

    QSizePolicy sp = _ui->shareLinkToolButton->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    _ui->shareLinkToolButton->setSizePolicy(sp);
    _ui->shareLinkToolButton->hide();

    //Is this a file or folder?
    QFileInfo fi(localPath);
    _isFile = fi.isFile();

    connect(_ui->enableShareLink, &QPushButton::clicked, this, &ShareLinkWidget::slotCreateShareLink);
    connect(_ui->lineEdit_password, &QLineEdit::returnPressed, this, &ShareLinkWidget::slotCreatePassword);
    connect(_ui->confirmPassword, &QAbstractButton::clicked, this, &ShareLinkWidget::slotCreatePassword);
    connect(_ui->confirmExpirationDate, &QAbstractButton::clicked, this, &ShareLinkWidget::slotSetExpireDate);
    connect(_ui->calendar, &QDateTimeEdit::dateChanged, this, &ShareLinkWidget::slotSetExpireDate);

    _ui->errorLabel->hide();

    bool sharingPossible = true;
    if (!_account->capabilities().sharePublicLink()) {
        qCWarning(lcShareLink) << "Link shares have been disabled";
        sharingPossible = false;
    } else if (!(maxSharingPermissions & SharePermissionShare)) {
        qCWarning(lcShareLink) << "The file can not be shared because it was shared without sharing permission.";
        sharingPossible = false;
    }

    _ui->enableShareLink->setChecked(false);
    _ui->shareLinkToolButton->setEnabled(false);
    _ui->shareLinkToolButton->hide();

    // Older servers don't support multiple public link shares
    if (!_account->capabilities().sharePublicLinkMultiple()) {
        _namesSupported = false;
    }

    togglePasswordOptions(false);
    toggleExpireDateOptions(false);
    _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));

    // check if the file is already inside of a synced folder
    if (sharePath.isEmpty()) {
        qCWarning(lcShareLink) << "Unable to share files not in a sync folder.";
        return;
    }
}

ShareLinkWidget::~ShareLinkWidget()
{
    delete _ui;
}

void ShareLinkWidget::slotToggleAnimation(bool start){
    if (start) {
        if (!_ui->progressIndicator->isAnimated())
            _ui->progressIndicator->startAnimation();
    } else {
        _ui->progressIndicator->stopAnimation();
    }
}

void ShareLinkWidget::setLinkShare(QSharedPointer<LinkShare> linkShare){
    _linkShare = linkShare;
}

QSharedPointer<LinkShare> ShareLinkWidget::getLinkShare(){
    return _linkShare;
}

void ShareLinkWidget::setupUiOptions(){
    connect(_linkShare.data(), &LinkShare::expireDateSet, this, &ShareLinkWidget::slotExpireDateSet);
    connect(_linkShare.data(), &LinkShare::passwordSet, this, &ShareLinkWidget::slotPasswordSet);
    connect(_linkShare.data(), &LinkShare::passwordSetError, this, &ShareLinkWidget::slotPasswordSetError);

    // Prepare permissions check and create group action
    const QDate expireDate = _linkShare.data()->getExpireDate().isValid()? _linkShare.data()->getExpireDate() : QDate();
    const SharePermissions perm = _linkShare.data()->getPermissions();
    bool checked = false;
    QActionGroup *permissionsGroup = new QActionGroup(this);

    // Prepare sharing menu
    _linkContextMenu = new QMenu(this);

    // radio button style
    permissionsGroup->setExclusive(true);

    if(_isFile){
        checked = perm & (SharePermissionRead & SharePermissionUpdate);
        _allowEditingLinkAction = _linkContextMenu->addAction(tr("Allow Editing"));
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

    if(_linkShare.data()->isPasswordSet()){
        _passwordProtectLinkAction->setChecked(true);
        _ui->lineEdit_password->setPlaceholderText("********");
        showPasswordOptions(true);
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
    if(!expireDate.isNull()){
        _ui->calendar->setDate(expireDate);
        _expirationDateLinkAction->setChecked(true);
        showExpireDateOptions(true);
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

    _linkContextMenu->addSeparator();

    _addAnotherLinkAction = _linkContextMenu->addAction(QIcon(":/client/resources/add.png"),
                                                         tr("Add another link"));

    _ui->enableShareLink->setIcon(QIcon(":/client/resources/copy.svg"));
    disconnect(_ui->enableShareLink, &QPushButton::clicked, this, &ShareLinkWidget::slotCreateShareLink);
    connect(_ui->enableShareLink, &QPushButton::clicked, this, &ShareLinkWidget::slotCopyLinkShare);

    connect(_linkContextMenu, &QMenu::triggered,
        this, &ShareLinkWidget::slotLinkContextMenuActionTriggered);

    _ui->shareLinkToolButton->setMenu(_linkContextMenu);
    _ui->shareLinkToolButton->setEnabled(true);
    _ui->enableShareLink->setEnabled(true);
    _ui->enableShareLink->setChecked(true);

    // show sharing options
    _ui->shareLinkToolButton->show();

    //TO DO
    //startAnimation(0, height());
}

void ShareLinkWidget::slotCopyLinkShare(bool clicked){
    Q_UNUSED(clicked);

    QApplication::clipboard()->setText(_linkShare->getLink().toString());
}

void ShareLinkWidget::slotExpireDateSet()
{
    slotToggleAnimation(false);
}

void ShareLinkWidget::slotSetExpireDate()
{
    if(!_linkShare){
        return;
    }

    slotToggleAnimation(true);
    _ui->errorLabel->hide();
    _linkShare->setExpireDate(_ui->calendar->date());
}

void ShareLinkWidget::slotCreatePassword()
{
    if (!_linkShare) {
        return;
    }

    slotToggleAnimation(true);
    _ui->errorLabel->hide();
    _linkShare->setPassword(_ui->lineEdit_password->text());
}

void ShareLinkWidget::slotCreateShareLink(bool clicked)
{
    slotToggleAnimation(true);
    emit createLinkShare();
}

void ShareLinkWidget::slotPasswordSet()
{
    _ui->lineEdit_password->setText(QString());
    if (_linkShare->isPasswordSet()) {
        _ui->lineEdit_password->setPlaceholderText("********");
        _ui->lineEdit_password->setEnabled(true);
    } else {
        _ui->lineEdit_password->setPlaceholderText(QString());
    }

    slotToggleAnimation(false);
}

void ShareLinkWidget::startAnimation(const int start, const int end){

    QPropertyAnimation *animation = new QPropertyAnimation(this, "maximumHeight", this);

    animation->setDuration(500);
    animation->setStartValue(start);
    animation->setEndValue(end);

    connect(animation, &QAbstractAnimation::finished, this, &ShareLinkWidget::slotAnimationFinished);
    if(end < start) // that is to remove the widget, not to show it
        connect(animation, &QAbstractAnimation::finished, this, &ShareLinkWidget::slotDeleteAnimationFinished);
    connect(animation, &QVariantAnimation::valueChanged, this, &ShareLinkWidget::resizeRequested);

    animation->start();
}

void ShareLinkWidget::slotDeleteShareFetched()
{
    slotToggleAnimation(false);

    // TODO
    //startAnimation(height(), 0);

    _linkShare.clear();
    togglePasswordOptions(false);
    toggleExpireDateOptions(false);
    emit deleteLinkShare();
}

void ShareLinkWidget::slotAnimationFinished()
{
    emit resizeRequested();
    deleteLater();
}

void ShareLinkWidget::slotDeleteAnimationFinished()
{
    // There is a painting bug where a small line of this widget isn't
    // properly cleared. This explicit repaint() call makes sure any trace of
    // the share widget is removed once it's destroyed. #4189
    connect(this, SIGNAL(destroyed(QObject *)), parentWidget(), SLOT(repaint()));
}

void ShareLinkWidget::slotCreateShareRequiresPassword(const QString &message)
{
    slotToggleAnimation(true);

    showPasswordOptions(true);
    if (!message.isEmpty()) {
        _ui->errorLabel->setText(message);
        _ui->errorLabel->show();
    }

    _passwordRequired = true;

    togglePasswordOptions(true);
}

void ShareLinkWidget::showPasswordOptions(bool show)
{
    _ui->passwordLabel->setVisible(show);
    _ui->lineEdit_password->setVisible(show);
    _ui->confirmPassword->setVisible(show);
}

void ShareLinkWidget::togglePasswordOptions(bool enable)
{
    showPasswordOptions(enable);

    if(enable) {
        _ui->lineEdit_password->setFocus();
    } else {
        // 'deletes' password
        if(_linkShare)
            _linkShare->setPassword(QString());
    }
}

void ShareLinkWidget::showExpireDateOptions(bool show)
{
    _ui->expirationLabel->setVisible(show);
    _ui->calendar->setVisible(show);
    _ui->confirmExpirationDate->setVisible(show);
}

void ShareLinkWidget::toggleExpireDateOptions(bool enable)
{
    showExpireDateOptions(enable);

    if (enable) {
        const QDate date = QDate::currentDate().addDays(1);
        _ui->calendar->setDate(date);
        _ui->calendar->setMinimumDate(date);
        _ui->calendar->setFocus();
    } else {
        // 'deletes' expire date
        if(_linkShare)
            _linkShare->setExpireDate(QDate());
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
        if (messageBox->clickedButton() == yesButton) {
            this->slotToggleAnimation(true);
            this->_linkShare->deleteShare();
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

    if(action == _addAnotherLinkAction){
        emit createLinkShare();

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
    slotToggleAnimation(false);

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
