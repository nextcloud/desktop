/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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


#include "accountsettings.h"
#include "ui_accountsettings.h"

#include "theme.h"
#include "folderman.h"
#include "folderwizard.h"
#include "folderstatusmodel.h"
#include "folderstatusdelegate.h"
#include "utility.h"
#include "application.h"
#include "configfile.h"
#include "account.h"
#include "accountstate.h"
#include "quotainfo.h"
#include "accountmanager.h"
#include "creds/abstractcredentials.h"

#include <math.h>

#include <QDebug>
#include <QDesktopServices>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QAction>
#include <QVBoxLayout>
#include <QTreeView>
#include <QKeySequence>
#include <QIcon>
#include <QVariant>
#include <qstringlistmodel.h>
#include <qpropertyanimation.h>

#include "account.h"

namespace OCC {

static const char progressBarStyleC[] =
        "QProgressBar {"
        "border: 1px solid grey;"
        "border-radius: 5px;"
        "text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "background-color: %1; width: 1px;"
        "}";

AccountSettings::AccountSettings(AccountState *accountState, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AccountSettings),
    _wasDisabledBefore(false),
    _accountState(accountState),
    _quotaInfo(accountState)
{
    ui->setupUi(this);

    _model = new FolderStatusModel;
    _model->setAccountState(_accountState);
    _model->setParent(this);
    FolderStatusDelegate *delegate = new FolderStatusDelegate;
    delegate->setParent(this);

    ui->_folderList->header()->hide();
    ui->_folderList->setItemDelegate( delegate );
    ui->_folderList->setModel( _model );
#if defined(Q_OS_MAC)
    ui->_folderList->setMinimumWidth( 400 );
#else
    ui->_folderList->setMinimumWidth( 300 );
#endif
    connect(ui->_folderList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotCustomContextMenuRequested(QPoint)));

    connect(ui->_folderList, SIGNAL(expanded(QModelIndex)) , this, SLOT(refreshSelectiveSyncStatus()));
    connect(ui->_folderList, SIGNAL(collapsed(QModelIndex)) , this, SLOT(refreshSelectiveSyncStatus()));
    connect(_model, SIGNAL(dirtyChanged()), this, SLOT(refreshSelectiveSyncStatus()));
    refreshSelectiveSyncStatus();

    QAction *resetFolderAction = new QAction(this);
    resetFolderAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(resetFolderAction, SIGNAL(triggered()), SLOT(slotResetCurrentFolder()));
    addAction(resetFolderAction);

    QAction *syncNowAction = new QAction(this);
    syncNowAction->setShortcut(QKeySequence(Qt::Key_F6));
    connect(syncNowAction, SIGNAL(triggered()), SLOT(slotSyncCurrentFolderNow()));
    addAction(syncNowAction);

    connect(ui->_folderList, SIGNAL(clicked(QModelIndex)), SLOT(slotFolderActivated(QModelIndex)));
    connect(ui->_folderList, SIGNAL(doubleClicked(QModelIndex)),SLOT(slotDoubleClicked(QModelIndex)));

    connect(ui->selectiveSyncApply, SIGNAL(clicked()), _model, SLOT(slotApplySelectiveSync()));
    connect(ui->selectiveSyncCancel, SIGNAL(clicked()), _model, SLOT(resetFolders()));
    connect(FolderMan::instance(), SIGNAL(folderListLoaded(Folder::Map)), _model, SLOT(resetFolders()));
    connect(this, SIGNAL(folderChanged()), _model, SLOT(resetFolders()));


    QColor color = palette().highlight().color();
    ui->quotaProgressBar->setStyleSheet(QString::fromLatin1(progressBarStyleC).arg(color.name()));
    ui->connectLabel->setWordWrap(true);
    ui->connectLabel->setOpenExternalLinks(true);
    QFont smallFont = ui->quotaInfoLabel->font();
    smallFont.setPointSize(smallFont.pointSize() * 0.8);
    ui->quotaInfoLabel->setFont(smallFont);

    _quotaLabel = new QLabel(ui->quotaProgressBar);
    (new QVBoxLayout(ui->quotaProgressBar))->addWidget(_quotaLabel);

    // This ensures the progress bar is big enough for the label.
    ui->quotaProgressBar->setMinimumHeight(_quotaLabel->height());

    ui->connectLabel->setText(tr("No account configured."));

    connect(_accountState, SIGNAL(stateChanged(int)), SLOT(slotAccountStateChanged(int)));
    slotAccountStateChanged(_accountState->state());

    connect( &_quotaInfo, SIGNAL(quotaUpdated(qint64,qint64)),
            this, SLOT(slotUpdateQuota(qint64,qint64)));

    connect(ui->deleteButton, SIGNAL(clicked()) , this, SLOT(slotDeleteAccount()));

}

void AccountSettings::slotCustomContextMenuRequested(const QPoint &pos)
{
    QTreeView *tv = ui->_folderList;
    QModelIndex index = tv->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    QString alias = _model->data( index, FolderStatusDelegate::FolderAliasRole ).toString();
    if (alias.isEmpty()) {
        return;
    }

    tv->setCurrentIndex(index);
    bool folderPaused = _model->data( index, FolderStatusDelegate::FolderSyncPaused).toBool();

    QMenu *menu = new QMenu(tv);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(tr("Remove folder")), SIGNAL(triggered(bool)),
            this, SLOT(slotRemoveCurrentFolder()));
    connect(menu->addAction(folderPaused ? tr("Resume") : tr("Pause")), SIGNAL(triggered(bool)),
            this, SLOT(slotEnableCurrentFolder()));
    menu->exec(tv->mapToGlobal(pos));
}

void AccountSettings::slotFolderActivated( const QModelIndex& indx )
{
    if (indx.data(FolderStatusDelegate::AddButton).toBool()) {
        slotAddFolder();
        return;
    }
}

void AccountSettings::slotAddFolder()
{
    FolderMan *folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(false); // do not start more syncs.

    FolderWizard *folderWizard = new FolderWizard(_accountState->account(), this);

    connect(folderWizard, SIGNAL(accepted()), SLOT(slotFolderWizardAccepted()));
    connect(folderWizard, SIGNAL(rejected()), SLOT(slotFolderWizardRejected()));
    folderWizard->open();
}


void AccountSettings::slotFolderWizardAccepted()
{
    FolderWizard *folderWizard = qobject_cast<FolderWizard*>(sender());
    FolderMan *folderMan = FolderMan::instance();

    qDebug() << "* Folder wizard completed";

    FolderDefinition definition;
    definition.alias        = folderWizard->field(QLatin1String("alias")).toString();
    definition.localPath    = folderWizard->field(QLatin1String("sourceFolder")).toString();
    definition.targetPath   = folderWizard->property("targetPath").toString();
    auto selectiveSyncBlackList = folderWizard->property("selectiveSyncBlackList").toStringList();

    folderMan->setSyncEnabled(true);

    Folder *f = folderMan->addFolder(_accountState, definition);
    if( f ) {
        f->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, selectiveSyncBlackList);
        folderMan->slotScheduleAllFolders();
        emit folderChanged();
    }
}

void AccountSettings::slotFolderWizardRejected()
{
    qDebug() << "* Folder wizard cancelled";
    FolderMan *folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(true);
}

void AccountSettings::setGeneralErrors( const QStringList& errors )
{
    _generalErrors = errors;
    if (_accountState) {
        // this will update the message
        slotAccountStateChanged(_accountState->state());
    }
}

void AccountSettings::slotRemoveCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if( selected.isValid() ) {
        int row = selected.row();

        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
        qDebug() << "Remove Folder alias " << alias;
        if( !alias.isEmpty() ) {
            // remove from file system through folder man
            // _model->removeRow( selected.row() );
            int ret = QMessageBox::question( this, tr("Confirm Folder Remove"),
                                             tr("<p>Do you really want to stop syncing the folder <i>%1</i>?</p>"
                                                "<p><b>Note:</b> This will not remove the files from your client.</p>").arg(alias),
                                             QMessageBox::Yes|QMessageBox::No );

            if( ret == QMessageBox::No ) {
                return;
            }

            FolderMan *folderMan = FolderMan::instance();
            folderMan->slotRemoveFolder( folderMan->folder(alias) );
            _model->removeRow(row);

            // single folder fix to show add-button and hide remove-button

            emit folderChanged();
        }
    }
}

void AccountSettings::slotResetCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if( selected.isValid() ) {
        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
        if (alias.isEmpty())
            return;
        int ret = QMessageBox::question( 0, tr("Confirm Folder Reset"),
                                         tr("<p>Do you really want to reset folder <i>%1</i> and rebuild your client database?</p>"
                                            "<p><b>Note:</b> This function is designed for maintenance purposes only. "
                                            "No files will be removed, but this can cause significant data traffic and "
                                            "take several minutes or hours to complete, depending on the size of the folder. "
                                            "Only use this option if advised by your administrator.</p>").arg(alias),
                                         QMessageBox::Yes|QMessageBox::No );
        if( ret == QMessageBox::Yes ) {
            FolderMan *folderMan = FolderMan::instance();
            Folder *f = folderMan->folder(alias);
            f->slotTerminateSync();
            f->wipe();
            folderMan->slotScheduleAllFolders();
        }
    }
}

void AccountSettings::slotDoubleClicked( const QModelIndex& indx )
{
    if( ! indx.isValid() ) return;
    QString alias = _model->data( indx, FolderStatusDelegate::FolderAliasRole ).toString();
    if (alias.isEmpty()) return;

    emit openFolderAlias( alias );
}

void AccountSettings::showConnectionLabel( const QString& message, const QString& tooltip )
{
    const QString errStyle = QLatin1String("color:#ffffff; background-color:#bb4d4d;padding:5px;"
                                           "border-width: 1px; border-style: solid; border-color: #aaaaaa;"
                                           "border-radius:5px;");
    if( _generalErrors.isEmpty() ) {
        ui->connectLabel->setText( message );
        ui->connectLabel->setToolTip(tooltip);
        ui->connectLabel->setStyleSheet(QString());
    } else {
        const QString msg = _generalErrors.join(QLatin1String("\n"));
        ui->connectLabel->setText( msg );
        ui->connectLabel->setToolTip(QString());
        ui->connectLabel->setStyleSheet(errStyle);
    }
    ui->accountStatus->setVisible(!message.isEmpty());
}

void AccountSettings::slotEnableCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();

    if( selected.isValid() ) {
        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();

        if( alias.isEmpty() ) {
            qDebug() << "Empty alias to enable.";
            return;
        }

        FolderMan *folderMan = FolderMan::instance();

        qDebug() << "Application: enable folder with alias " << alias;
        bool terminate = false;
        bool currentlyPaused = false;

        // this sets the folder status to disabled but does not interrupt it.
        Folder *f = folderMan->folder( alias );
        if (!f) {
            return;
        }
        currentlyPaused = f->syncPaused();
        if( ! currentlyPaused ) {
            // check if a sync is still running and if so, ask if we should terminate.
            if( f->isBusy() ) { // its still running
#if defined(Q_OS_MAC)
                QWidget *parent = this;
                Qt::WindowFlags flags = Qt::Sheet;
#else
                QWidget *parent = 0;
                Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint; // default flags
#endif
                QMessageBox msgbox(QMessageBox::Question, tr("Sync Running"),
                                   tr("The syncing operation is running.<br/>Do you want to terminate it?"),
                                   QMessageBox::Yes | QMessageBox::No, parent, flags);
                msgbox.setDefaultButton(QMessageBox::Yes);
                int reply = msgbox.exec();
                if ( reply == QMessageBox::Yes )
                    terminate = true;
                else
                    return; // do nothing
            }
        }

        // message box can return at any time while the thread keeps running,
        // so better check again after the user has responded.
        if ( f->isBusy() && terminate ) {
            f->slotTerminateSync();
        }
        f->setSyncPaused(!currentlyPaused); // toggle the pause setting
        folderMan->slotSetFolderPaused( f, !currentlyPaused );

        // keep state for the icon setting.
        if( currentlyPaused ) _wasDisabledBefore = true;

        _model->slotUpdateFolderState (f);
    }
}

void AccountSettings::slotSyncCurrentFolderNow()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if( !selected.isValid() )
        return;
    QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
    FolderMan *folderMan = FolderMan::instance();

    folderMan->slotScheduleSync(folderMan->folder(alias));
}

void AccountSettings::slotOpenOC()
{
  if( _OCUrl.isValid() )
    QDesktopServices::openUrl( _OCUrl );
}

void AccountSettings::slotUpdateQuota(qint64 total, qint64 used)
{
    if( total > 0 ) {
        ui->storageGroupBox->setVisible(true);
        ui->quotaProgressBar->setVisible(true);
        ui->quotaInfoLabel->setVisible(true);
        ui->quotaProgressBar->setEnabled(true);
        // workaround the label only accepting ints (which may be only 32 bit wide)
        ui->quotaProgressBar->setMaximum(100);
        int qVal = qRound(used/(double)total * 100);
        if( qVal > 100 ) qVal = 100;
        ui->quotaProgressBar->setValue(qVal);
        QString usedStr = Utility::octetsToString(used);
        QString totalStr = Utility::octetsToString(total);
        double percent = used/(double)total*100;
        QString percentStr = Utility::compactFormatDouble(percent, 1);
        _quotaLabel->setText(tr("%1 (%3%) of %2 server space in use.").arg(usedStr, totalStr, percentStr));
    } else {
        ui->storageGroupBox->setVisible(false);
        ui->quotaInfoLabel->setVisible(false);
        ui->quotaProgressBar->setMaximum(0);
        _quotaLabel->setText(tr("Currently there is no storage usage information available."));
    }
}

void AccountSettings::slotAccountStateChanged(int state)
{
    if (_accountState) {
        ui->sslButton->updateAccountState(_accountState);
        AccountPtr account = _accountState->account();
        QUrl safeUrl(account->url());
        safeUrl.setPassword(QString()); // Remove the password from the URL to avoid showing it in the UI
        FolderMan *folderMan = FolderMan::instance();
        foreach (Folder *folder, folderMan->map().values()) {
            _model->slotUpdateFolderState(folder);
        }
        if (state == AccountState::Connected || state == AccountState::ServiceUnavailable) {
            QString user;
            if (AbstractCredentials *cred = account->credentials()) {
               user = cred->user();
            }
            if (user.isEmpty()) {
                showConnectionLabel( tr("Connected to <a href=\"%1\">%2</a>.").arg(account->url().toString(), safeUrl.toString())
                                 /*, tr("Version: %1 (%2)").arg(versionStr).arg(version) */ );
            } else {
                showConnectionLabel( tr("Connected to <a href=\"%1\">%2</a> as <i>%3</i>.").arg(account->url().toString(), safeUrl.toString(), user)
                                 /*, tr("Version: %1 (%2)").arg(versionStr).arg(version) */ );
            }
        } else {
            showConnectionLabel( tr("No connection to %1 at <a href=\"%2\">%3</a>.")
                                 .arg(Theme::instance()->appNameGUI(),
                                      account->url().toString(),
                                      safeUrl.toString()) );
        }
    } else {
        // ownCloud is not yet configured.
        showConnectionLabel( tr("No %1 connection configured.").arg(Theme::instance()->appNameGUI()) );
    }
}

AccountSettings::~AccountSettings()
{
    delete ui;
}

void AccountSettings::refreshSelectiveSyncStatus()
{
    bool shouldBeVisible = _model->isDirty();
    QStringList undecidedFolder;
    for (int i = 0; !shouldBeVisible && i < _model->rowCount(); ++i) {
        if (ui->_folderList->isExpanded(_model->index(i)))
            shouldBeVisible = true;
    }

    foreach (Folder *folder, FolderMan::instance()->map().values()) {
        if (folder->accountState() != _accountState) { continue; }
        auto undecidedList =  folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList);
        foreach(const auto &it, undecidedList) {
            undecidedFolder += ( folder->alias() + QLatin1String("/") + it);
        }
    }
    if (undecidedFolder.isEmpty()) {
        ui->selectiveSyncNotification->setText(QString());
    } else {
        ui->selectiveSyncNotification->setText(
            tr("There are new shared folders that were not synchronized because they are too big: %1")
                .arg(undecidedFolder.join(tr(", "))));
        shouldBeVisible = true;
    }

    ui->selectiveSyncApply->setEnabled(_model->isDirty() || !undecidedFolder.isEmpty());
    ui->selectiveSyncCancel->setEnabled(_model->isDirty());
    bool wasVisible = !ui->selectiveSyncStatus->isHidden();
    if (wasVisible != shouldBeVisible) {
        QSize hint = ui->selectiveSyncStatus->sizeHint();
        if (shouldBeVisible) {
            ui->selectiveSyncStatus->setMaximumHeight(0);
            ui->selectiveSyncStatus->setVisible(true);
        }
        auto anim = new QPropertyAnimation(ui->selectiveSyncStatus, "maximumHeight", ui->selectiveSyncStatus);
        anim->setEndValue(shouldBeVisible ? hint.height() : 0);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        if (!shouldBeVisible) {
            connect(anim, SIGNAL(finished()), ui->selectiveSyncStatus, SLOT(hide()));
        }
    }
}

void AccountSettings::slotDeleteAccount()
{
    int ret = QMessageBox::question( this, tr("Confirm Account Delete"),
                                     tr("<p>Do you really want to delete the account <i>%1</i>?</p>"
                                     "<p><b>Note:</b> This will not remove the files from your client.</p>")
                                        .arg(_accountState->account()->displayName()),
                                     QMessageBox::Yes|QMessageBox::No );

    if( ret == QMessageBox::No ) {
        return;
    }

    auto manager = AccountManager::instance();
    manager->deleteAccount(_accountState);
    manager->save();
}

bool AccountSettings::event(QEvent* e)
{
    if (e->type() == QEvent::Hide || e->type() == QEvent::Show) {
        _quotaInfo.setActive(isVisible());
    }
    return QWidget::event(e);
}

} // namespace OCC
