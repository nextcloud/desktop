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

#include "shareusergroupwidget.h"
#include "ui_shareusergroupwidget.h"
#include "ui_sharewidget.h"
#include "account.h"
#include "json.h"
#include "folderman.h"
#include "folder.h"
#include "accountmanager.h"
#include "theme.h"
#include "configfile.h"
#include "capabilities.h"

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
#include <qscrollarea.h>
#include <qlayout.h>
#include <QPropertyAnimation>
#include <QMenu>
#include <QAction>

namespace OCC {

ShareUserGroupWidget::ShareUserGroupWidget(AccountPtr account,
                                           const QString &sharePath,
                                           const QString &localPath,
                                           SharePermissions maxSharingPermissions,
                                           QWidget *parent) :
   QWidget(parent),
    _ui(new Ui::ShareUserGroupWidget),
    _account(account),
    _sharePath(sharePath),
    _localPath(localPath),
    _maxSharingPermissions(maxSharingPermissions),
    _disableCompleterActivated(false)
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
    connect(_completerModel, SIGNAL(shareesReady()), this, SLOT(slotShareesReady()));
    connect(_completerModel, SIGNAL(displayErrorMessage(int,QString)), this, SLOT(displayError(int,QString)));

    _completer->setModel(_completerModel);
    _completer->setCaseSensitivity(Qt::CaseInsensitive);
    _completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    _ui->shareeLineEdit->setCompleter(_completer);

    _manager = new ShareManager(_account, this);
    connect(_manager, SIGNAL(sharesFetched(QList<QSharedPointer<Share>>)), SLOT(slotSharesFetched(QList<QSharedPointer<Share>>)));
    connect(_manager, SIGNAL(shareCreated(QSharedPointer<Share>)), SLOT(getShares()));
    connect(_manager, SIGNAL(serverError(int,QString)), this, SLOT(displayError(int,QString)));
    connect(_ui->shareeLineEdit, SIGNAL(returnPressed()), SLOT(slotLineEditReturn()));

    // By making the next two QueuedConnections we can override
    // the strings the completer sets on the line edit.
    connect(_completer, SIGNAL(activated(QModelIndex)), SLOT(slotCompleterActivated(QModelIndex)),
            Qt::QueuedConnection);
    connect(_completer, SIGNAL(highlighted(QModelIndex)), SLOT(slotCompleterHighlighted(QModelIndex)),
            Qt::QueuedConnection);

    // Queued connection so this signal is recieved after textChanged
    connect(_ui->shareeLineEdit, SIGNAL(textEdited(QString)),
            this, SLOT(slotLineEditTextEdited(QString)), Qt::QueuedConnection);
    connect(&_completionTimer, SIGNAL(timeout()), this, SLOT(searchForSharees()));
    _completionTimer.setSingleShot(true);
    _completionTimer.setInterval(600);

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    _ui->errorLabel->hide();

    // Setup the sharee search progress indicator
    _ui->shareeHorizontalLayout->addWidget(&_pi_sharee);
}

ShareUserGroupWidget::~ShareUserGroupWidget()
{
    delete _ui;
}

void ShareUserGroupWidget::on_shareeLineEdit_textChanged(const QString &)
{
    _completionTimer.stop();
}

void ShareUserGroupWidget::slotLineEditTextEdited(const QString& text)
{
    _disableCompleterActivated = false;
    // First textChanged is called first and we stopped the timer when the text is changed, programatically or not
    // Then we restart the timer here if the user touched a key
    if (!text.isEmpty()) {
        _completionTimer.start();
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

    foreach (auto sw, _ui->scrollArea->findChildren<ShareWidget*>()) {
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
    QScrollArea *scrollArea = _ui->scrollArea;

    auto newViewPort = new QWidget(scrollArea);
    auto layout = new QVBoxLayout(newViewPort);

    QSize minimumSize = newViewPort->sizeHint();
    int x = 0;

    foreach(const auto &share, shares) {
        // We don't handle link shares
        if (share->getShareType() == Share::TypeLink) {
            continue;
        }

        ShareWidget *s = new ShareWidget(share, _maxSharingPermissions, _isFile, _ui->scrollArea);
        connect(s, SIGNAL(resizeRequested()), this, SLOT(slotAdjustScrollWidgetSize()));
        layout->addWidget(s);

        x++;
        if (x <= 3) {
            minimumSize = newViewPort->sizeHint();
        } else {
            minimumSize.rwidth() = qMax(newViewPort->sizeHint().width(), minimumSize.width());
        }
    }

    minimumSize.rwidth() += layout->spacing();
    minimumSize.rheight() += layout->spacing();
    scrollArea->setMinimumSize(minimumSize);
    scrollArea->setVisible(!shares.isEmpty());
    scrollArea->setWidget(newViewPort);

    _disableCompleterActivated = false;
    _ui->shareeLineEdit->setEnabled(true);
}

void ShareUserGroupWidget::slotAdjustScrollWidgetSize()
{
    QScrollArea *scrollArea = _ui->scrollArea;
    if (scrollArea->findChildren<ShareWidget*>().count() <= 3) {
        auto minimumSize = scrollArea->widget()->sizeHint();
        auto spacing = scrollArea->widget()->layout()->spacing();
        minimumSize.rwidth() += spacing;
        minimumSize.rheight() += spacing;
        scrollArea->setMinimumSize(minimumSize);
    }
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

void ShareUserGroupWidget::slotCompleterActivated(const QModelIndex & index)
{
    if (_disableCompleterActivated)
        return;
    // The index is an index from the QCompletion model which is itelf a proxy
    // model proxying the _completerModel
    auto sharee = qvariant_cast<QSharedPointer<Sharee>>(index.data(Qt::UserRole));
    if (sharee.isNull()) {
        return;
    }

    /*
     * Add spinner to the bottom of the widget list
     */
    auto viewPort = _ui->scrollArea->widget();
    auto layout = viewPort->layout();
    auto indicator = new QProgressIndicator(viewPort);
    indicator->startAnimation();
    layout->addWidget(indicator);

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
        _manager->createShare(_sharePath, Share::ShareType(sharee->type()),
                              sharee->shareWith(), SharePermissionDefault);
    }

    _ui->shareeLineEdit->setEnabled(false);
    _ui->shareeLineEdit->setText(QString());
}

void ShareUserGroupWidget::slotCompleterHighlighted(const QModelIndex & index)
{
    // By default the completer would set the text to EditRole,
    // override that here.
    _ui->shareeLineEdit->setText(index.data(Qt::DisplayRole).toString());
}

void ShareUserGroupWidget::displayError(int code, const QString& message)
{
    _pi_sharee.stopAnimation();
    qDebug() << "Error from server" << code << message;
    _ui->errorLabel->setText(message);
    _ui->errorLabel->show();
}

ShareWidget::ShareWidget(QSharedPointer<Share> share,
                         SharePermissions maxSharingPermissions,
                         bool isFile,
                         QWidget *parent) :
  QWidget(parent),
  _ui(new Ui::ShareWidget),
  _share(share),
  _isFile(isFile)
{
    _ui->setupUi(this);

    _ui->sharedWith->setText(share->getShareWith()->format());
 
    // Create detailed permissions menu
    QMenu *menu = new QMenu(this);
    _permissionCreate = new QAction(tr("create"), this);
    _permissionCreate->setCheckable(true);
    _permissionCreate->setEnabled(maxSharingPermissions & SharePermissionCreate);
    _permissionUpdate = new QAction(tr("change"), this);
    _permissionUpdate->setCheckable(true);
    _permissionUpdate->setEnabled(maxSharingPermissions & SharePermissionUpdate);
    _permissionDelete = new QAction(tr("delete"), this);
    _permissionDelete->setCheckable(true);
    _permissionDelete->setEnabled(maxSharingPermissions & SharePermissionDelete);

    menu->addAction(_permissionUpdate);
    /*
     * Files can't have create or delete permissions
     */
    if (!_isFile) {
        menu->addAction(_permissionCreate);
        menu->addAction(_permissionDelete);
    }
    _ui->permissionToolButton->setMenu(menu);
    _ui->permissionToolButton->setPopupMode(QToolButton::InstantPopup);

    QIcon icon(QLatin1String(":/client/resources/more.png"));
    _ui->permissionToolButton->setIcon(icon);

    // If there's only a single entry in the detailed permission menu, hide it
    if (menu->actions().size() == 1) {
        _ui->permissionToolButton->hide();
    }

    // Set the permissions checkboxes
    displayPermissions();

    _ui->permissionShare->setEnabled(maxSharingPermissions & SharePermissionShare);
    _ui->permissionsEdit->setEnabled(maxSharingPermissions
            & (SharePermissionCreate | SharePermissionUpdate | SharePermissionDelete));

    connect(_permissionUpdate, SIGNAL(triggered(bool)), SLOT(slotPermissionsChanged()));
    connect(_permissionCreate, SIGNAL(triggered(bool)), SLOT(slotPermissionsChanged()));
    connect(_permissionDelete, SIGNAL(triggered(bool)), SLOT(slotPermissionsChanged()));
    connect(_ui->permissionShare,  SIGNAL(clicked(bool)), SLOT(slotPermissionsChanged()));
    connect(_ui->permissionsEdit,  SIGNAL(clicked(bool)), SLOT(slotEditPermissionsChanged()));

    /*
     * We don't show permssion share for federated shares with server <9.1
     * https://github.com/owncloud/core/issues/22122#issuecomment-185637344
     * https://github.com/owncloud/client/issues/4996
     */
    if (share->getShareType() == Share::TypeRemote
            && share->account()->serverVersionInt() < Account::makeServerVersion(9, 1, 0)) {
        _ui->permissionShare->setVisible(false);
        _ui->permissionToolButton->setVisible(false);
    }

    connect(share.data(), SIGNAL(permissionsSet()), SLOT(slotPermissionsSet()));
    connect(share.data(), SIGNAL(shareDeleted()), SLOT(slotShareDeleted()));

    _ui->deleteShareButton->setIcon(QIcon::fromTheme(QLatin1String("user-trash"),
                                                     QIcon(QLatin1String(":/client/resources/delete.png"))));

    if (!share->account()->capabilities().shareResharing()) {
        _ui->permissionShare->hide();
    }
}

void ShareWidget::on_deleteShareButton_clicked()
{
    setEnabled(false);
    _share->deleteShare();
}

ShareWidget::~ShareWidget()
{
    delete _ui;
}

void ShareWidget::slotEditPermissionsChanged()
{
    setEnabled(false);

    Share::Permissions permissions = SharePermissionRead;

    if (_ui->permissionShare->checkState() == Qt::Checked) {
        permissions |= SharePermissionShare;
    }
    
    if (_ui->permissionsEdit->checkState() == Qt::Checked) {
        if (_permissionUpdate->isEnabled())
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

void ShareWidget::slotPermissionsChanged()
{
    setEnabled(false);
    
    Share::Permissions permissions = SharePermissionRead;

    if (_permissionUpdate->isChecked()) {
        permissions |= SharePermissionUpdate;
    }

    if (_permissionCreate->isChecked()) {
        permissions |= SharePermissionCreate;
    }

    if (_permissionDelete->isChecked()) {
        permissions |= SharePermissionDelete;
    }

    if (_ui->permissionShare->checkState() == Qt::Checked) {
        permissions |= SharePermissionShare;
    }

    _share->setPermissions(permissions);
}

void ShareWidget::slotDeleteAnimationFinished()
{
    resizeRequested();
    deleteLater();

    // There is a painting bug where a small line of this widget isn't
    // properly cleared. This explicit repaint() call makes sure any trace of
    // the share widget is removed once it's destroyed. #4189
    connect(this, SIGNAL(destroyed(QObject*)), parentWidget(), SLOT(repaint()));
}

void ShareWidget::slotShareDeleted()
{
    QPropertyAnimation *animation = new QPropertyAnimation(this, "maximumHeight", this);

    animation->setDuration(500);
    animation->setStartValue(height());
    animation->setEndValue(0);

    connect(animation, SIGNAL(finished()), SLOT(slotDeleteAnimationFinished()));
    connect(animation, SIGNAL(valueChanged(QVariant)), this, SIGNAL(resizeRequested()));

    animation->start();
}

void ShareWidget::slotPermissionsSet()
{
    displayPermissions();
    setEnabled(true);
}

QSharedPointer<Share> ShareWidget::share() const
{
    return _share;
}

void ShareWidget::displayPermissions()
{
    _permissionCreate->setChecked(false);
    _ui->permissionsEdit->setCheckState(Qt::Unchecked);
    _permissionDelete->setChecked(false);
    _ui->permissionShare->setCheckState(Qt::Unchecked);
    _permissionUpdate->setChecked(false);

    if (_share->getPermissions() & SharePermissionUpdate) {
        _permissionUpdate->setChecked(true);
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }
    if (!_isFile && _share->getPermissions() & SharePermissionCreate) {
        _permissionCreate->setChecked(true);
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }
    if (!_isFile && _share->getPermissions() & SharePermissionDelete) {
        _permissionDelete->setChecked(true);
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }
    if (_share->getPermissions() & SharePermissionShare) {
        _ui->permissionShare->setCheckState(Qt::Checked);
    }
}

}
