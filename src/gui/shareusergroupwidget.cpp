/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#include "share.h"
#include "sharee.h"

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

namespace OCC {

ShareUserGroupWidget::ShareUserGroupWidget(AccountPtr account, const QString &sharePath, const QString &localPath, bool resharingAllowed, QWidget *parent) :
   QWidget(parent),
    _ui(new Ui::ShareUserGroupWidget),
    _account(account),
    _sharePath(sharePath),
    _localPath(localPath),
    _resharingAllowed(resharingAllowed)
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

    _completer->setModel(_completerModel);
    _ui->shareeLineEdit->setCompleter(_completer);

    _manager = new ShareManager(_account, this);
    connect(_manager, SIGNAL(sharesFetched(QList<QSharedPointer<Share>>)), SLOT(slotSharesFetched(QList<QSharedPointer<Share>>)));
    connect(_manager, SIGNAL(shareCreated(QSharedPointer<Share>)), SLOT(getShares()));
//    connect(_ui->shareeLineEdit, SIGNAL(returnPressed()), SLOT(on_searchPushButton_clicked()));
    connect(_completer, SIGNAL(activated(QModelIndex)), SLOT(slotCompleterActivated(QModelIndex)));

    // Queued connection so this signal is recieved after textChanged
    connect(_ui->shareeLineEdit, SIGNAL(textEdited(QString)),
            this, SLOT(slotLineEditTextEdited(QString)), Qt::QueuedConnection);
    connect(&_completionTimer, SIGNAL(timeout()), this, SLOT(searchForSharees()));
    _completionTimer.setSingleShot(true);
    _completionTimer.setInterval(600);

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
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
    // First textChanged is called first and we stopped the timer when the text is changed, programatically or not
    // Then we restart the timer here if the user touched a key
    if (!text.isEmpty()) {
        _completionTimer.start();
    }
}


void ShareUserGroupWidget::searchForSharees()
{
    _completionTimer.stop();
    ShareeModel::ShareeSet blacklist;

    // Add the current user to _sharees since we can't share with ourself
    QSharedPointer<Sharee> currentUser(new Sharee(_account->credentials()->user(), "", Sharee::Type::User));
    blacklist << currentUser;

    foreach (auto sw, _ui->scrollArea->findChildren<ShareWidget*>()) {
        blacklist << sw->share()->getShareWith();
    }

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

        ShareWidget *s = new ShareWidget(share, _ui->scrollArea);
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
    _completer->complete();
}

void ShareUserGroupWidget::slotCompleterActivated(const QModelIndex & index)
{
    // The index is an index from the QCompletion model which is itelf a proxy
    // model proxying the _completerModel
    auto sharee = qvariant_cast<QSharedPointer<Sharee>>(index.data(Qt::UserRole));
    if (sharee.isNull()) {
        return;
    }

    _manager->createShare(_sharePath, Share::ShareType(sharee->type()),
                          sharee->shareWith(), Share::PermissionDefault);

    _ui->shareeLineEdit->setText(QString());
}

ShareWidget::ShareWidget(QSharedPointer<Share> share,
                                   QWidget *parent) :
  QWidget(parent),
  _ui(new Ui::ShareWidget),
  _share(share),
  _showDetailedPermissions(false)
{
    _ui->setupUi(this);

    _ui->sharedWith->setText(share->getShareWith()->format());

    // Set the permissions checkboxes
    displayPermissions();

    // Hide "detailed permissions" by default
    _ui->permissionDelete->setHidden(true);
    _ui->permissionUpdate->setHidden(true);
    _ui->permissionCreate->setHidden(true);

    connect(_ui->permissionUpdate, SIGNAL(clicked(bool)), SLOT(slotPermissionsChanged()));
    connect(_ui->permissionCreate, SIGNAL(clicked(bool)), SLOT(slotPermissionsChanged()));
    connect(_ui->permissionDelete, SIGNAL(clicked(bool)), SLOT(slotPermissionsChanged()));
    connect(_ui->permissionShare,  SIGNAL(clicked(bool)), SLOT(slotPermissionsChanged()));
    connect(_ui->permissionsEdit,  SIGNAL(clicked(bool)), SLOT(slotEditPermissionsChanged()));

    connect(share.data(), SIGNAL(permissionsSet()), SLOT(slotPermissionsSet()));
    connect(share.data(), SIGNAL(shareDeleted()), SLOT(slotShareDeleted()));

    _ui->deleteShareButton->setIcon(QIcon::fromTheme(QLatin1String("user-trash"),
                                                     QIcon(QLatin1String(":/client/resources/delete.png"))));
}

void ShareWidget::on_deleteShareButton_clicked()
{
    setEnabled(false);
    _share->deleteShare();
}

void ShareWidget::on_permissionToggleButton_clicked()
{
    _showDetailedPermissions = !_showDetailedPermissions;
    _ui->permissionDelete->setVisible(_showDetailedPermissions);
    _ui->permissionUpdate->setVisible(_showDetailedPermissions);
    _ui->permissionCreate->setVisible(_showDetailedPermissions);

    if (_showDetailedPermissions) {
        _ui->permissionToggleButton->setText("Hide");
    } else {
        _ui->permissionToggleButton->setText("More");
    }
    emit resizeRequested();
}

ShareWidget::~ShareWidget()
{
    delete _ui;
}

void ShareWidget::slotEditPermissionsChanged()
{
    setEnabled(false);

    Share::Permissions permissions = Share::PermissionRead;

    if (_ui->permissionShare->checkState() == Qt::Checked) {
        permissions |= Share::PermissionUpdate;
    }
    
    if (_ui->permissionsEdit->checkState() == Qt::Checked) {
        permissions |= Share::PermissionCreate;
        permissions |= Share::PermissionUpdate;
        permissions |= Share::PermissionDelete;
    }

    _share->setPermissions(permissions);
}

void ShareWidget::slotPermissionsChanged()
{
    setEnabled(false);
    
    Share::Permissions permissions = Share::PermissionRead;

    if (_ui->permissionUpdate->checkState() == Qt::Checked) {
        permissions |= Share::PermissionUpdate;
    }

    if (_ui->permissionCreate->checkState() == Qt::Checked) {
        permissions |= Share::PermissionCreate;
    }

    if (_ui->permissionDelete->checkState() == Qt::Checked) {
        permissions |= Share::PermissionDelete;
    }

    if (_ui->permissionShare->checkState() == Qt::Checked) {
        permissions |= Share::PermissionShare;
    }

    _share->setPermissions(permissions);
}

void ShareWidget::slotDeleteAnimationFinished()
{
    resizeRequested();
    deleteLater();
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
    _ui->permissionCreate->setCheckState(Qt::Unchecked);
    _ui->permissionsEdit->setCheckState(Qt::Unchecked);
    _ui->permissionDelete->setCheckState(Qt::Unchecked);
    _ui->permissionShare->setCheckState(Qt::Unchecked);
    _ui->permissionUpdate->setCheckState(Qt::Unchecked);

    if (_share->getPermissions() & Share::PermissionUpdate) {
        _ui->permissionUpdate->setCheckState(Qt::Checked);
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }
    if (_share->getPermissions() & Share::PermissionCreate) {
        _ui->permissionCreate->setCheckState(Qt::Checked);
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }
    if (_share->getPermissions() & Share::PermissionDelete) {
        _ui->permissionDelete->setCheckState(Qt::Checked);
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }
    if (_share->getPermissions() & Share::PermissionShare) {
        _ui->permissionShare->setCheckState(Qt::Checked);
    }
}

}
