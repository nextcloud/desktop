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
#include <QMenu>
#include <QAction>

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
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
    _completer->setFilterMode(Qt::MatchContains);
#endif
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

        ShareWidget *s = new ShareWidget(share, _isFile, _ui->scrollArea);
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
    menu->addAction(_permissionCreate);
    _permissionUpdate = new QAction(tr("change"), this);
    _permissionUpdate->setCheckable(true);
    menu->addAction(_permissionUpdate);
    _permissionDelete = new QAction(tr("delete"), this);
    _permissionDelete->setCheckable(true);
    if (!_isFile) {
        menu->addAction(_permissionDelete);
    }
    _ui->permissionToolButton->setMenu(menu);
    _ui->permissionToolButton->setPopupMode(QToolButton::InstantPopup);

    // Set the permissions checkboxes
    displayPermissions();

    connect(_permissionUpdate, SIGNAL(triggered(bool)), SLOT(slotPermissionsChanged()));
    connect(_permissionCreate, SIGNAL(triggered(bool)), SLOT(slotPermissionsChanged()));
    connect(_permissionDelete, SIGNAL(triggered(bool)), SLOT(slotPermissionsChanged()));
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

ShareWidget::~ShareWidget()
{
    delete _ui;
}

void ShareWidget::slotEditPermissionsChanged()
{
    setEnabled(false);

    Share::Permissions permissions = Share::PermissionRead;

    if (_ui->permissionShare->checkState() == Qt::Checked) {
        permissions |= Share::PermissionShare;
    }
    
    if (_ui->permissionsEdit->checkState() == Qt::Checked) {
        permissions |= Share::PermissionCreate;
        permissions |= Share::PermissionUpdate;

        if (!_isFile) {
            permissions |= Share::PermissionDelete;
        }
    }

    _share->setPermissions(permissions);
}

void ShareWidget::slotPermissionsChanged()
{
    setEnabled(false);
    
    Share::Permissions permissions = Share::PermissionRead;

    if (_permissionUpdate->isChecked()) {
        permissions |= Share::PermissionUpdate;
    }

    if (_permissionCreate->isChecked()) {
        permissions |= Share::PermissionCreate;
    }

    if (_permissionDelete->isChecked()) {
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
    _permissionCreate->setChecked(false);
    _ui->permissionsEdit->setCheckState(Qt::Unchecked);
    _permissionDelete->setChecked(false);
    _ui->permissionShare->setCheckState(Qt::Unchecked);
    _permissionUpdate->setChecked(false);

    if (_share->getPermissions() & Share::PermissionUpdate) {
        _permissionUpdate->setChecked(true);
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }
    if (_share->getPermissions() & Share::PermissionCreate) {
        _permissionCreate->setChecked(true);
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }
    if (!_isFile && _share->getPermissions() & Share::PermissionDelete) {
        _permissionDelete->setChecked(true);
        _ui->permissionsEdit->setCheckState(Qt::Checked);
    }
    if (_share->getPermissions() & Share::PermissionShare) {
        _ui->permissionShare->setCheckState(Qt::Checked);
    }
}

}
