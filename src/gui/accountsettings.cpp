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
#include "owncloudsetupwizard.h"
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
    connect(_model, SIGNAL(suggestExpand(QModelIndex)), ui->_folderList, SLOT(expand(QModelIndex)));
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

    connect(ui->selectiveSyncApply, SIGNAL(clicked()), _model, SLOT(slotApplySelectiveSync()));
    connect(ui->selectiveSyncCancel, SIGNAL(clicked()), _model, SLOT(resetFolders()));
    connect(FolderMan::instance(), SIGNAL(folderListLoaded(Folder::Map)), _model, SLOT(resetFolders()));
    connect(this, SIGNAL(folderChanged()), _model, SLOT(resetFolders()));


    QColor color = palette().highlight().color();
    ui->quotaProgressBar->setStyleSheet(QString::fromLatin1(progressBarStyleC).arg(color.name()));

    ui->connectLabel->setText(tr("No account configured."));

    connect(_accountState, SIGNAL(stateChanged(int)), SLOT(slotAccountStateChanged(int)));
    slotAccountStateChanged(_accountState->state());

    connect( &_quotaInfo, SIGNAL(quotaUpdated(qint64,qint64)),
            this, SLOT(slotUpdateQuota(qint64,qint64)));

    connect(ui->deleteButton, SIGNAL(clicked()) , this, SLOT(slotDeleteAccount()));

    // Expand already on single click
    ui->_folderList->setExpandsOnDoubleClick(false);
    QObject::connect(ui->_folderList, SIGNAL(clicked(const QModelIndex &)),
        ui->_folderList, SLOT(expand(const QModelIndex &)));
}

void AccountSettings::doExpand()
{
    ui->_folderList->expandToDepth(0);
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
    connect(menu->addAction(tr("Open folder")), SIGNAL(triggered(bool)),
            this, SLOT(slotOpenCurrentFolder()));
    connect(menu->addAction(tr("Choose What to Sync")), SIGNAL(triggered(bool)),
            this, SLOT(doExpand()));
    connect(menu->addAction(folderPaused ? tr("Resume sync") : tr("Pause sync")), SIGNAL(triggered(bool)),
            this, SLOT(slotEnableCurrentFolder()));
    connect(menu->addAction(tr("Remove sync")), SIGNAL(triggered(bool)),
            this, SLOT(slotRemoveCurrentFolder()));
    menu->exec(tv->mapToGlobal(pos));
}

void AccountSettings::slotFolderActivated( const QModelIndex& indx )
{
    if (indx.data(FolderStatusDelegate::AddButton).toBool()
            && indx.flags() & Qt::ItemIsEnabled) {
        slotAddFolder();
        return;
    }
    if (_model->classify(indx) == FolderStatusModel::RootFolder) {
        // tries to find if we clicked on the '...' button.
        QTreeView *tv = ui->_folderList;
        auto pos = tv->mapFromGlobal(QCursor::pos());
        if (FolderStatusDelegate::optionsButtonRect(tv->visualRect(indx)).contains(pos)) {
            slotCustomContextMenuRequested(pos);
        }
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
    definition.localPath    = FolderDefinition::prepareLocalPath(
            folderWizard->field(QLatin1String("sourceFolder")).toString());
    definition.targetPath   = folderWizard->property("targetPath").toString();

    {
        QDir dir(definition.localPath);
        if (!dir.exists()) {
            qDebug() << "Creating folder" << definition.localPath;
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this, tr("Folder creation failed"),
                                     tr("<p>Could not create local folder <i>%1</i>.")
                                        .arg(QDir::toNativeSeparators(definition.localPath)));
                return;
            }

        }
    }

    /* take the value from the definition of already existing folders. All folders have
     * the same setting so far.
     * The default is to not sync hidden files
     */
    definition.ignoreHiddenFiles = folderMan->ignoreHiddenFiles();

    auto selectiveSyncBlackList = folderWizard->property("selectiveSyncBlackList").toStringList();

    folderMan->setSyncEnabled(true);

    Folder *f = folderMan->addFolder(_accountState, definition);
    if( f ) {
        f->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, selectiveSyncBlackList);

        // The user already accepted the selective sync dialog. everything is in the white list
        f->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList,
                                             QStringList() << QLatin1String("/"));
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

void AccountSettings::slotRemoveCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if( selected.isValid() ) {
        int row = selected.row();

        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
        qDebug() << "Remove Folder alias " << alias;
        if( !alias.isEmpty() ) {
            QMessageBox messageBox(QMessageBox::Question,
                                   tr("Confirm Sync Removal"),
                                   tr("<p>Do you really want to stop syncing the folder <i>%1</i>?</p>"
                                      "<p><b>Note:</b> This will <b>not</b> delete any files.</p>").arg(alias),
                                   QMessageBox::NoButton,
                                   this);
            QPushButton* yesButton =
                    messageBox.addButton(tr("Stop syncing"), QMessageBox::YesRole);
            messageBox.addButton(tr("Cancel"), QMessageBox::NoRole);

            messageBox.exec();
            if (messageBox.clickedButton() != yesButton) {
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
            if(Folder *f = folderMan->folder(alias)) {
                f->slotTerminateSync();
                f->wipe();
            }
            folderMan->slotScheduleAllFolders();
        }
    }
}

void AccountSettings::slotOpenCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();

    if( selected.isValid() ) {
        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
        emit openFolderAlias(alias);
    }
}

void AccountSettings::showConnectionLabel( const QString& message, QStringList errors )
{
    const QString errStyle = QLatin1String("color:#ffffff; background-color:#bb4d4d;padding:5px;"
                                           "border-width: 1px; border-style: solid; border-color: #aaaaaa;"
                                           "border-radius:5px;");
    if( errors.isEmpty() ) {
        ui->connectLabel->setText( message );
        ui->connectLabel->setToolTip(QString());
        ui->connectLabel->setStyleSheet(QString());
    } else {
        errors.prepend(message);
        const QString msg = errors.join(QLatin1String("\n"));
        qDebug() << msg;
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
        ui->quotaProgressBar->setVisible(true);
        ui->quotaProgressBar->setEnabled(true);
        // workaround the label only accepting ints (which may be only 32 bit wide)
        const double percent = used/(double)total*100;
        const int percentInt = qMin(qRound(percent), 100);
        ui->quotaProgressBar->setValue(percentInt);
        QString usedStr = Utility::octetsToString(used);
        QString totalStr = Utility::octetsToString(total);
        QString percentStr = Utility::compactFormatDouble(percent, 1);
        QString toolTip = tr("%1 (%3%) of %2 in use. Some folders, including network mounted or shared folders, might have different limits.").arg(usedStr, totalStr, percentStr);
        ui->quotaInfoLabel->setText(tr("%1 of %2 in use").arg(usedStr, totalStr));
        ui->quotaInfoLabel->setToolTip(toolTip);
        ui->quotaProgressBar->setToolTip(toolTip);
    } else {
        ui->quotaProgressBar->setVisible(false);
        ui->quotaInfoLabel->setText(tr("Currently there is no storage usage information available."));
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

        QString server = QString::fromLatin1("<a href=\"%1\">%2</a>").arg(account->url().toString(), safeUrl.toString());
        QString serverWithUser = server;
        if (AbstractCredentials *cred = account->credentials()) {
           serverWithUser = tr("%1 as <i>%2</i>").arg(server, cred->user());
        }

        if (state == AccountState::Connected) {
            showConnectionLabel( tr("Connected to %1.").arg(serverWithUser) );
        } else if (state == AccountState::ServiceUnavailable) {
            showConnectionLabel( tr("Server %1 is temporarily unavailable.").arg(server) );
        } else if (state == AccountState::SignedOut) {
            showConnectionLabel( tr("Signed out from %1.").arg(serverWithUser) );
        } else {
            showConnectionLabel( tr("No connection to %1 at %2.")
                                 .arg(Theme::instance()->appNameGUI(),
                                      server), _accountState->connectionErrors() );
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
        if (folder->accountState() != _accountState) {
            continue;
        }

        auto undecidedList =  folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList);
        foreach(const auto &it, undecidedList) {
            undecidedFolder.append(it);
        }
    }

    if (undecidedFolder.isEmpty()) {
        ui->selectiveSyncNotification->setVisible(false);
        ui->selectiveSyncNotification->setText(QString());
    } else {
        ui->selectiveSyncNotification->setVisible(true);
        ui->selectiveSyncNotification->setText(
            tr("There are new folders that were not synchronized because they are too big: %1")
                .arg(undecidedFolder.join(tr(", "))));
        shouldBeVisible = true;
    }

    ui->selectiveSyncApply->setEnabled(_model->isDirty() || !undecidedFolder.isEmpty());
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
    // Deleting the account potentially deletes 'this', so
    // the QMessageBox should be destroyed before that happens.
    {
        QMessageBox messageBox(QMessageBox::Question,
                               tr("Confirm Account Removal"),
                               tr("<p>Do you really want to remove the connection to the account <i>%1</i>?</p>"
                                  "<p><b>Note:</b> This will <b>not</b> delete any files.</p>")
                                 .arg(_accountState->account()->displayName()),
                               QMessageBox::NoButton,
                               this);
        QPushButton* yesButton =
                messageBox.addButton(tr("Remove connection"), QMessageBox::YesRole);
        messageBox.addButton(tr("Cancel"), QMessageBox::NoRole);

        messageBox.exec();
        if (messageBox.clickedButton() != yesButton) {
            return;
        }
    }

    auto manager = AccountManager::instance();
    manager->deleteAccount(_accountState);
    manager->save();

    // if there is no more account, show the wizard.
    if( manager->accounts().isEmpty() ) {
        OwncloudSetupWizard::runWizard(qApp, SLOT(slotownCloudWizardDone(int)));
    }

}

bool AccountSettings::event(QEvent* e)
{
    if (e->type() == QEvent::Hide || e->type() == QEvent::Show) {
        _quotaInfo.setActive(isVisible());
    }
    return QWidget::event(e);
}

} // namespace OCC
