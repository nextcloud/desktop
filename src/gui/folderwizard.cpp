/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include "folderwizard.h"
#include "folderman.h"
#include "configfile.h"
#include "theme.h"
#include "networkjobs.h"
#include "account.h"
#include "selectivesyncdialog.h"
#include "accountstate.h"
#include "buttonstyle.h"
#include "creds/abstractcredentials.h"
#include "SesComponents/syncdirvalidation.h"
#include "wizard/owncloudwizard.h"
#include "common/asserts.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QUrl>
#include <QValidator>
#include <QWizardPage>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QEvent>
#include <QCheckBox>
#include <QMessageBox>
#include <QStandardPaths>

#include <cstdlib>

namespace
{
constexpr QColor darkWarnYellow(63, 63, 0);
constexpr QColor lightWarnYellow(255, 255, 192);

QPalette yellowWarnWidgetPalette(const QPalette &existingPalette)
{
    const auto warnYellow = OCC::Theme::instance()->darkMode() ? darkWarnYellow : lightWarnYellow;
    auto modifiedPalette = existingPalette;
    modifiedPalette.setColor(QPalette::Window, warnYellow);
    modifiedPalette.setColor(QPalette::Base, warnYellow);
    return modifiedPalette;
}
}

namespace OCC {

QString FormatWarningsWizardPage::formatWarnings(const QStringList &warnings) const
{
    QString formattedWarning;
    if (warnings.count() == 1) {
        formattedWarning = tr("%1").arg(warnings.first());
    } else if (warnings.count() > 1) {
        formattedWarning = tr("") + " <ul>";
        for (const auto &warning : warnings) {
            formattedWarning += QString::fromLatin1("<li>%1</li>").arg(warning);
        }
        formattedWarning += "</ul>";
    }

    return formattedWarning;
}

FolderWizardLocalPath::FolderWizardLocalPath(const AccountPtr &account)
    : FormatWarningsWizardPage()
    , _account(account)
{
    _ui.setupUi(this);
    registerField(QLatin1String("sourceFolder*"), _ui.localFolderLineEdit);
    connect(_ui.localFolderChooseBtn, &QAbstractButton::clicked, this, &FolderWizardLocalPath::slotChooseLocalFolder);
    _ui.localFolderChooseBtn->setToolTip(tr("Click to select a local folder to sync."));

    QUrl serverUrl = _account->url();
    serverUrl.setUserName(_account->credentials()->user());
    QString defaultPath = QDir::homePath() + QLatin1Char('/') + Theme::instance()->appName();
    defaultPath = FolderMan::instance()->findGoodPathForNewSyncFolder(defaultPath, serverUrl, FolderMan::GoodPathStrategy::AllowOnlyNewPath);
    _ui.localFolderLineEdit->setText(QDir::toNativeSeparators(defaultPath));
    _ui.localFolderLineEdit->setToolTip(tr("Enter the path to the local folder."));

    _ui.sesSnackBar->setWordWrap(true);
    _ui.sesSnackBar->hide();

    _ui.localFolderChooseBtn->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));

    changeStyle();
}

FolderWizardLocalPath::~FolderWizardLocalPath() = default;

void FolderWizardLocalPath::initializePage()
{
    _ui.sesSnackBar->hide();
}

void FolderWizardLocalPath::cleanupPage()
{
    _ui.sesSnackBar->hide();
}

bool FolderWizardLocalPath::isComplete() const
{
    QUrl serverUrl = _account->url();
    serverUrl.setUserName(_account->credentials()->user());


    SyncDirValidator syncDirValidator(_ui.localFolderLineEdit->text());
    if (!syncDirValidator.isValidDir()) {
        _ui.sesSnackBar->show();
        _ui.sesSnackBar->setError(syncDirValidator.message());
        return false;
    }

    const auto errorStr = FolderMan::instance()->checkPathValidityForNewFolder(
        QDir::fromNativeSeparators(_ui.localFolderLineEdit->text()), serverUrl).second;

    if(errorStr.isEmpty())
    {
        _ui.sesSnackBar->hide();
        _ui.sesSnackBar->clearMessage();
        return true;
    }
    else
    {
         _ui.sesSnackBar->show();
        _ui.sesSnackBar->setWarning(formatWarnings(QStringList(errorStr)));
        return false;
    }
}

void FolderWizardLocalPath::slotChooseLocalFolder()
{
    QString sf = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir d(sf);

    // open the first entry of the home dir. Otherwise the dir picker comes
    // up with the closed home dir icon, stupid Qt default...
    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
        QDir::DirsFirst | QDir::Name);

    if (dirs.count() > 0)
        sf += "/" + dirs.at(0); // Take the first dir in home dir.

    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select the source folder"),
        sf);

     SyncDirValidator syncDirValidator(QDir::fromNativeSeparators("\\ //"));
    // SyncDirValidator syncDirValidator(QDir::fromNativeSeparators(_ui.localFolderLineEdit->text()));
    if (!syncDirValidator.isValidDir() && !dir.isEmpty()) {
        _ui.sesSnackBar->show();
        _ui.sesSnackBar->setError(syncDirValidator.message());
        return;
    }
    if (!dir.isEmpty()) {
        // set the last directory component name as alias
        _ui.localFolderLineEdit->setText(QDir::toNativeSeparators(dir));
    }
    emit completeChanged();
}


void FolderWizardLocalPath::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        // Notify the other widgets (Dark-/Light-Mode switching)
        changeStyle();
        break;
    default:
        break;
    }

    FormatWarningsWizardPage::changeEvent(e);
}

void FolderWizardLocalPath::changeStyle()
{
    _ui.title->setStyleSheet(IonosTheme::fontConfigurationCss(
            IonosTheme::settingsFont(),
            IonosTheme::settingsBigTitleSize(),
            IonosTheme::settingsTitleWeight600(),
            IonosTheme::titleColor()));

    _ui.title->setProperty("text", tr("Add Folder Sync"));

    _ui.subTitle->setStyleSheet(IonosTheme::fontConfigurationCss(
            IonosTheme::settingsFont(),
            IonosTheme::settingsTextSize(),
            IonosTheme::settingsTitleWeight600(),
            IonosTheme::folderWizardSubtitleColor()));

    _ui.subTitle->setProperty("text", tr("Step 1 of 3: Select local folder"));

    _ui.description->setStyleSheet(IonosTheme::fontConfigurationCss(
            IonosTheme::settingsFont(),
            IonosTheme::settingsTextSize(),
            IonosTheme::settingsTextWeight(),
            IonosTheme::titleColor()));

    _ui.description->setProperty("text",
        tr("Select a folder on your hard drive that should be permanetly connected to your %1. All files and "
        "subfolders are automatically uploaded and synchronized").arg(Theme::instance()->appNameGUI()));

    _ui.localFolderLineEdit->setStyleSheet(QString(
        "color: %1; font-family: %2; font-size: %3; font-weight: %4; border-radius: %5; border: 1px "
        "solid %6; padding: 0px 12px; text-align: left; vertical-align: middle; height: 40px; background: %7; ")
        .arg(IonosTheme::folderWizardPathColor())
        .arg(IonosTheme::settingsFont())
        .arg(IonosTheme::settingsTextSize())
        .arg(IonosTheme::settingsTextWeight())
        .arg(IonosTheme::buttonRadius())
        .arg(IonosTheme::menuBorderColor())
        .arg(IonosTheme::white())
    );

    _ui.localFolderChooseBtn->setProperty("text", tr("Choose"));


#if defined(Q_OS_MAC)
    _ui.localFolderChooseBtn->setStyleSheet(
        QStringLiteral("QPushButton { margin-left: 5px; margin-top: 12px; height: 40px; width: 80px; %1} ").arg(
            IonosTheme::fontConfigurationCss(
                IonosTheme::settingsFont(),
                IonosTheme::settingsTextSize(),
                IonosTheme::settingsTitleWeight500(),
                IonosTheme::white()
            )
        )
    );
#endif

}

// =================================================================================
FolderWizardRemotePath::FolderWizardRemotePath(const AccountPtr &account)
    : FormatWarningsWizardPage()
    , _account(account)
{
    _ui.setupUi(this);
    _ui.sesSnackBar->hide();

    _ui.folderTreeWidget->setSortingEnabled(true);
    _ui.folderTreeWidget->sortByColumn(0, Qt::AscendingOrder);

    connect(_ui.addFolderButton, &QAbstractButton::clicked, this, &FolderWizardRemotePath::slotAddRemoteFolder);
    connect(_ui.refreshButton, &QAbstractButton::clicked, this, &FolderWizardRemotePath::slotRefreshFolders);
    connect(_ui.folderTreeWidget, &QTreeWidget::itemExpanded, this, &FolderWizardRemotePath::slotItemExpanded);
    connect(_ui.folderTreeWidget, &QTreeWidget::currentItemChanged, this, &FolderWizardRemotePath::slotCurrentItemChanged);
    connect(_ui.folderEntry, &QLineEdit::textEdited, this, &FolderWizardRemotePath::slotFolderEntryEdited);

    _ui.refreshButton->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));
    _ui.addFolderButton->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));

    _ui.buttonLayout->setAlignment(Qt::AlignLeft);

    _lscolTimer.setInterval(500);
    _lscolTimer.setSingleShot(true);
    connect(&_lscolTimer, &QTimer::timeout, this, &FolderWizardRemotePath::slotLsColFolderEntry);

    _ui.folderTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

#ifdef Q_OS_MAC
    _ui.folderTreeWidget->setPalette(QPalette(IonosTheme::white()));
#endif

    // Make sure that there will be a scrollbar when the contents is too wide
    _ui.folderTreeWidget->header()->setStretchLastSection(false);

    changeStyle();
}

void FolderWizardRemotePath::slotAddRemoteFolder()
{
    QTreeWidgetItem *current = _ui.folderTreeWidget->currentItem();

    QString parent('/');
    if (current) {
        parent = current->data(0, Qt::UserRole).toString();
    }

    auto *dlg = new QInputDialog(this);

    dlg->setWindowTitle(tr("Create Remote Folder"));
    dlg->setLabelText(tr("Enter the name of the new folder to be created below \"%1\":")
                          .arg(parent));
    dlg->open(this, SLOT(slotCreateRemoteFolder(QString)));

    QDialogButtonBox *buttonBox = dlg->findChild<QDialogButtonBox*>();
    buttonBox->setLayoutDirection(Qt::RightToLeft);
    buttonBox->layout()->setSpacing(16);
    buttonBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    buttonBox->button(QDialogButtonBox::Ok)->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    dlg->findChild<QLineEdit*>()->setStyleSheet(
        QStringLiteral(
            "color: %1; font-family: %2; font-size: %3; font-weight: %4; border-radius: %5; border: 1px "
            "solid %6; padding: 0px 12px; text-align: left; vertical-align: middle; height: 40px; background: %7; ").arg(
                IonosTheme::folderWizardPathColor(),
                IonosTheme::settingsFont(),
                IonosTheme::settingsTextSize(),
                IonosTheme::settingsTextWeight(),
                IonosTheme::buttonRadius(),
                IonosTheme::menuBorderColor(),
                IonosTheme::white()
            )
    );

    #ifdef Q_OS_MAC
        buttonBox->layout()->setSpacing(24);

        dlg->setStyleSheet(
            QStringLiteral(" %1; } ").arg(
                IonosTheme::fontConfigurationCss(
                    IonosTheme::settingsFont(),
                    IonosTheme::settingsTextSize(),
                    IonosTheme::settingsTextWeight(),
                    IonosTheme::titleColor()
                )
            )
        );

        buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(
            buttonBox->button(QDialogButtonBox::Ok)->styleSheet() +
            QStringLiteral(" color: %1; ").arg(IonosTheme::white())
        );
    #endif
}

void FolderWizardRemotePath::slotCreateRemoteFolder(const QString &folder)
{
    if (folder.isEmpty())
        return;

    QTreeWidgetItem *current = _ui.folderTreeWidget->currentItem();
    QString fullPath;
    if (current) {
        fullPath = current->data(0, Qt::UserRole).toString();
    }
    fullPath += "/" + folder;

    auto *job = new MkColJob(_account, fullPath, this);
    /* check the owncloud configuration file and query the ownCloud */
    connect(job, &MkColJob::finishedWithoutError,
        this, &FolderWizardRemotePath::slotCreateRemoteFolderFinished);
    connect(job, &AbstractNetworkJob::networkError, this, &FolderWizardRemotePath::slotHandleMkdirNetworkError);
    job->start();
}

void FolderWizardRemotePath::slotCreateRemoteFolderFinished()
{
    qCDebug(lcWizard) << "webdav mkdir request finished";
    showSuccess(tr("Folder was successfully created on %1.").arg(Theme::instance()->appNameGUI()));
    slotRefreshFolders();
    _ui.folderEntry->setText(dynamic_cast<MkColJob *>(sender())->path());
    slotLsColFolderEntry();
}

void FolderWizardRemotePath::slotHandleMkdirNetworkError(QNetworkReply *reply)
{
    qCWarning(lcWizard) << "webdav mkdir request failed:" << reply->error();
    if (!_account->credentials()->stillValid(reply)) {
        showError(tr("Authentication failed accessing %1").arg(Theme::instance()->appNameGUI()));
    } else {
        showError(tr("Failed to create the folder on %1. Please check manually.")
                     .arg(Theme::instance()->appNameGUI()));
    }
}

void FolderWizardRemotePath::slotHandleLsColNetworkError(QNetworkReply *reply)
{
    // Ignore 404s, otherwise users will get annoyed by error popups
    // when not typing fast enough. It's still clear that a given path
    // was not found, because the 'Next' button is disabled and no entry
    // is selected in the tree view.
    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpCode == 404) {
        showWarn(QString()); // hides the warning pane
        return;
    }
    auto job = qobject_cast<LsColJob *>(sender());
    ASSERT(job);
    showError(tr("Failed to list a folder. Error: %1")
                 .arg(job->errorStringParsingBody()));
}

static QTreeWidgetItem *findFirstChild(QTreeWidgetItem *parent, const QString &text)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->text(0) == text) {
            return child;
        }
    }
    return nullptr;
}

void FolderWizardRemotePath::recursiveInsert(QTreeWidgetItem *parent, QStringList pathTrail, QString path)
{
    if (pathTrail.isEmpty())
        return;

    const QString parentPath = parent->data(0, Qt::UserRole).toString();
    const QString folderName = pathTrail.first();
    QString folderPath;
    if (parentPath == QLatin1String("/")) {
        folderPath = folderName;
    } else {
        folderPath = parentPath + "/" + folderName;
    }
    QTreeWidgetItem *item = findFirstChild(parent, folderName);
    if (!item) {
        item = new QTreeWidgetItem(parent);
        QFileIconProvider prov;
        QIcon folderIcon = prov.icon(QFileIconProvider::Folder);
        item->setIcon(0, folderIcon);
        item->setText(0, folderName);
        item->setData(0, Qt::UserRole, folderPath);
        item->setToolTip(0, folderPath);
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }

    pathTrail.removeFirst();
    recursiveInsert(item, pathTrail, path);
}

bool FolderWizardRemotePath::selectByPath(QString path)
{
    if (path.startsWith(QLatin1Char('/'))) {
        path = path.mid(1);
    }
    if (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }

    QTreeWidgetItem *it = _ui.folderTreeWidget->topLevelItem(0);
    if (!path.isEmpty()) {
        const QStringList pathTrail = path.split(QLatin1Char('/'));
        foreach (const QString &path, pathTrail) {
            if (!it) {
                return false;
            }
            it = findFirstChild(it, path);
        }
    }
    if (!it) {
        return false;
    }

    _ui.folderTreeWidget->setCurrentItem(it);
    _ui.folderTreeWidget->scrollToItem(it);
    return true;
}

void FolderWizardRemotePath::slotUpdateDirectories(const QStringList &list)
{
    QString webdavFolder = QUrl(_account->davUrl()).path();

    QTreeWidgetItem *root = _ui.folderTreeWidget->topLevelItem(0);
    if (!root) {
        root = new QTreeWidgetItem(_ui.folderTreeWidget);
        root->setText(0, Theme::instance()->appNameGUI());
        root->setIcon(0, Theme::instance()->applicationIcon());
        root->setToolTip(0, tr("Choose this to sync the entire account"));
        root->setData(0, Qt::UserRole, "/");
    }
    QStringList sortedList = list;
    Utility::sortFilenames(sortedList);
    foreach (QString path, sortedList) {
        path.remove(webdavFolder);

        // Don't allow to select subfolders of encrypted subfolders
        const auto isAnyAncestorEncrypted = std::any_of(std::cbegin(_encryptedPaths), std::cend(_encryptedPaths), [=](const QString &encryptedPath) {
            return path.size() > encryptedPath.size() && path.startsWith(encryptedPath);
        });
        if (isAnyAncestorEncrypted) {
            continue;
        }

        QStringList paths = path.split('/');
        if (paths.last().isEmpty())
            paths.removeLast();
        recursiveInsert(root, paths, path);
    }
    root->setExpanded(true);
}

void FolderWizardRemotePath::slotGatherEncryptedPaths(const QString &path, const QMap<QString, QString> &properties)
{
    const auto it = properties.find("is-encrypted");
    if (it == properties.cend() || *it != QStringLiteral("1")) {
        return;
    }

    const auto webdavFolder = QUrl(_account->davUrl()).path();
    Q_ASSERT(path.startsWith(webdavFolder));
    _encryptedPaths << path.mid(webdavFolder.size());
}

void FolderWizardRemotePath::slotRefreshFolders()
{
    _encryptedPaths.clear();
    runLsColJob("/");
    _ui.folderTreeWidget->clear();
    _ui.folderEntry->clear();
}

void FolderWizardRemotePath::slotItemExpanded(QTreeWidgetItem *item)
{
    QString dir = item->data(0, Qt::UserRole).toString();
    runLsColJob(dir);
}

void FolderWizardRemotePath::slotCurrentItemChanged(QTreeWidgetItem *item)
{
    if (item) {
        QString dir = item->data(0, Qt::UserRole).toString();

        // We don't want to allow creating subfolders in encrypted folders outside of the sync logic
        const auto encrypted = _encryptedPaths.contains(dir);
        _ui.addFolderButton->setEnabled(!encrypted);

        if (!dir.startsWith(QLatin1Char('/'))) {
            dir.prepend(QLatin1Char('/'));
        }
        _ui.folderEntry->setText(dir);
    }

    emit completeChanged();
}

void FolderWizardRemotePath::slotFolderEntryEdited(const QString &text)
{
    if (selectByPath(text)) {
        _lscolTimer.stop();
        return;
    }

    _ui.folderTreeWidget->setCurrentItem(nullptr);
    _lscolTimer.start(); // avoid sending a request on each keystroke
}

void FolderWizardRemotePath::slotLsColFolderEntry()
{
    QString path = _ui.folderEntry->text();
    if (path.startsWith(QLatin1Char('/')))
        path = path.mid(1);

    LsColJob *job = runLsColJob(path);
    // No error handling, no updating, we do this manually
    // because of extra logic in the typed-path case.
    disconnect(job, nullptr, this, nullptr);
    connect(job, &LsColJob::finishedWithError,
        this, &FolderWizardRemotePath::slotHandleLsColNetworkError);
    connect(job, &LsColJob::directoryListingSubfolders,
        this, &FolderWizardRemotePath::slotTypedPathFound);
}

void FolderWizardRemotePath::slotTypedPathFound(const QStringList &subpaths)
{
    slotUpdateDirectories(subpaths);
    selectByPath(_ui.folderEntry->text());
}

LsColJob *FolderWizardRemotePath::runLsColJob(const QString &path)
{
    auto *job = new LsColJob(_account, path);
    const auto props = QList<QByteArray>() << "resourcetype"
                                           << "http://nextcloud.org/ns:is-encrypted";
    job->setProperties(props);
    connect(job, &LsColJob::directoryListingSubfolders,
        this, &FolderWizardRemotePath::slotUpdateDirectories);
    connect(job, &LsColJob::finishedWithError,
        this, &FolderWizardRemotePath::slotHandleLsColNetworkError);
    connect(job, &LsColJob::directoryListingIterated,
        this, &FolderWizardRemotePath::slotGatherEncryptedPaths);
    job->start();

    return job;
}

FolderWizardRemotePath::~FolderWizardRemotePath() = default;

bool FolderWizardRemotePath::isComplete() const
{
    if (!_ui.folderTreeWidget->currentItem()) {
        return false;
    }

    auto targetPath = _ui.folderTreeWidget->currentItem()->data(0, Qt::UserRole).toString();
    if (!targetPath.startsWith(QLatin1Char('/'))) {
        targetPath.prepend(QLatin1Char('/'));
    }
    wizard()->setProperty("targetPath", targetPath);

    for (const auto folder : std::as_const(FolderMan::instance()->map())) {
        if (folder->accountState()->account() != _account) {
            continue;
        }

        const auto remoteDir = folder->remotePathTrailingSlash();
        const auto localDir = folder->cleanPath();
        if (QDir::cleanPath(targetPath) == QDir::cleanPath(remoteDir)) {
            showWarn(tr("Please choose a different location. %1 is already being synced to %2.").arg(Utility::escape(remoteDir), Utility::escape(localDir)));
            break;
        }

        if (targetPath.startsWith(remoteDir)) {
            showWarn(tr("Please choose a different location. %1 is already being synced to %2.").arg(Utility::escape(targetPath), Utility::escape(localDir)));
            break;
        }

        if (remoteDir.startsWith(targetPath)) {
            showWarn(tr("Please choose a different location. %1 is already being synced to %2.").arg(Utility::escape(remoteDir), Utility::escape(localDir)));
            break;
        }
    }

    return true;
}

void FolderWizardRemotePath::cleanupPage()
{
    showWarn();
}

void FolderWizardRemotePath::initializePage()
{
    showWarn();
    slotRefreshFolders();
}

void FolderWizardRemotePath::showWarn(const QString &msg) const
{
    if (msg.isEmpty()) {
        _ui.sesSnackBar->hide();

    } else {
        _ui.sesSnackBar->show();
        _ui.sesSnackBar->setWarning(msg);
    }
}

void FolderWizardRemotePath::showSuccess(const QString &msg) const
{
    if (msg.isEmpty()) {
        _ui.sesSnackBar->hide();

    } else {
        _ui.sesSnackBar->show();
        _ui.sesSnackBar->setSuccess(msg);
    }
}

void FolderWizardRemotePath::showError(const QString &msg) const
{
    if (msg.isEmpty()) {
        _ui.sesSnackBar->hide();

    } else {
        _ui.sesSnackBar->show();
        _ui.sesSnackBar->setError(msg);
    }
}

void FolderWizardRemotePath::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        // Notify the other widgets (Dark-/Light-Mode switching)
        changeStyle();
        break;
    default:
        break;
    }

    FormatWarningsWizardPage::changeEvent(e);
}

void FolderWizardRemotePath::changeStyle()
{
    _ui.title->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsBigTitleSize(),
        IonosTheme::settingsTitleWeight600(),
        IonosTheme::titleColor()));

    _ui.title->setProperty("text", tr("Add Folder Sync"));

    _ui.subTitle->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTitleWeight600(),
        IonosTheme::folderWizardSubtitleColor()));

    _ui.subTitle->setProperty("text", tr("Step 2 of 3: Directory in your %1").arg(Theme::instance()->appNameGUI()));

    _ui.description1->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::titleColor()));

    _ui.description1->setProperty("text",
        tr("Please now select or create a target folder in your %1 where the content should be uploaded and synchronized.").arg(Theme::instance()->appNameGUI()));

    _ui.description2->setProperty("text",
        tr("Both folders are permanently linked and the respective contents are automatically synchronized and updated."));

    _ui.description2->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::titleColor()));

    _ui.folderEntry->setStyleSheet(
        QStringLiteral("color: %1; font-family: %2; font-size: %3; font-weight: %4; border-radius: %5; border: 1px "
        "solid %6; padding: 0px 12px; text-align: left; vertical-align: middle; height: 40px;")
        .arg(IonosTheme::folderWizardPathColor())
        .arg(IonosTheme::settingsFont())
        .arg(IonosTheme::settingsTextSize())
        .arg(IonosTheme::settingsTextWeight())
        .arg(IonosTheme::buttonRadius())
        .arg(IonosTheme::menuBorderColor()));

    _ui.folderTreeWidget->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::titleColor()));

    _ui.refreshButton->setProperty("text", tr("Refresh"));

    _ui.addFolderButton->setProperty("text", tr("Create folder"));

#if defined(Q_OS_MAC)
    _ui.buttonLayout->setSpacing(24);
#endif
}

// ====================================================================================

FolderWizardSelectiveSync::FolderWizardSelectiveSync(const AccountPtr &account)
{
    _uiSelectiveSync.setupUi(this);
    auto *layout = _uiSelectiveSync.verticalLayout;
    _selectiveSync = new SelectiveSyncWidget(account, this);
    layout->addWidget(_selectiveSync);

    if (Theme::instance()->showVirtualFilesOption() && bestAvailableVfsMode() != Vfs::Off) {
        _virtualFilesCheckBox = new QCheckBox(tr("Use virtual files instead of downloading content immediately %1").arg(bestAvailableVfsMode() == Vfs::WindowsCfApi ? QString() : tr("(experimental)")));

        connect(_virtualFilesCheckBox, &QCheckBox::clicked, this, &FolderWizardSelectiveSync::virtualFilesCheckboxClicked);
        connect(_virtualFilesCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
            _selectiveSync->setEnabled(state == Qt::Unchecked);
        });
        _virtualFilesCheckBox->setChecked(bestAvailableVfsMode() == Vfs::WindowsCfApi);
        _virtualFilesCheckBox->setStyleSheet("margin-top: 5px;");

        layout->addWidget(_virtualFilesCheckBox);

        _virtualFilesCheckBox->setStyleSheet(IonosTheme::fontConfigurationCss(
            IonosTheme::settingsFont(),
            IonosTheme::settingsTextSize(),
            IonosTheme::settingsTextWeight(),
            IonosTheme::titleColor()));
    }

    _selectiveSync->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::titleColor()));

    _uiSelectiveSync.title->setStyleSheet(IonosTheme::fontConfigurationCss(
            IonosTheme::settingsFont(),
            IonosTheme::settingsBigTitleSize(),
            IonosTheme::settingsTitleWeight600(),
            IonosTheme::titleColor()));
    _uiSelectiveSync.title->setProperty("text", tr("Add Folder Sync"));

    _uiSelectiveSync.subTitle->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTitleWeight600(),
        IonosTheme::folderWizardSubtitleColor()));

    _uiSelectiveSync.subTitle->setProperty("text", tr("Step 3 of 3: Selektive Synchronisation"));

}

FolderWizardSelectiveSync::~FolderWizardSelectiveSync() = default;


void FolderWizardSelectiveSync::initializePage()
{
    QString targetPath = wizard()->property("targetPath").toString();
    if (targetPath.startsWith('/')) {
        targetPath = targetPath.mid(1);
    }
    QString alias = QFileInfo(targetPath).fileName();
    if (alias.isEmpty())
        alias = Theme::instance()->appName();
    QStringList initialBlacklist;
    if (Theme::instance()->wizardSelectiveSyncDefaultNothing()) {
        initialBlacklist = QStringList("/");
    }
    _selectiveSync->setFolderInfo(targetPath, alias, initialBlacklist);

    if (_virtualFilesCheckBox) {
        // TODO: remove when UX decision is made
        if (Utility::isPathWindowsDrivePartitionRoot(wizard()->field(QStringLiteral("sourceFolder")).toString())) {
            _virtualFilesCheckBox->setChecked(false);
            _virtualFilesCheckBox->setEnabled(false);
            _virtualFilesCheckBox->setText(tr("Virtual files are not supported for Windows partition roots as local folder. Please choose a valid subfolder under drive letter."));
        } else {
            _virtualFilesCheckBox->setChecked(bestAvailableVfsMode() == Vfs::WindowsCfApi);
            _virtualFilesCheckBox->setEnabled(true);
            _virtualFilesCheckBox->setText(tr("Use virtual files instead of downloading content immediately %1").arg(bestAvailableVfsMode() == Vfs::WindowsCfApi ? QString() : tr("(experimental)")));

            if (Theme::instance()->enforceVirtualFilesSyncFolder()) {
                _virtualFilesCheckBox->setChecked(true);
                _virtualFilesCheckBox->setDisabled(true);
            }
        }
        //
    }

    QWizardPage::initializePage();
}

bool FolderWizardSelectiveSync::validatePage()
{
    const auto mode = bestAvailableVfsMode();
    const bool useVirtualFiles = (mode == Vfs::WindowsCfApi) && (_virtualFilesCheckBox && _virtualFilesCheckBox->isChecked());
    if (useVirtualFiles) {
        const auto availability = Vfs::checkAvailability(wizard()->field(QStringLiteral("sourceFolder")).toString(), mode);
        if (!availability) {
            auto msg = new QMessageBox(QMessageBox::Warning,
                                       tr("Virtual files are not supported at the selected location"),
                                       availability.error(),
                                       QMessageBox::Ok, this);
            msg->setAttribute(Qt::WA_DeleteOnClose);
            msg->open();
            return false;
        }
    }
    wizard()->setProperty("selectiveSyncBlackList", useVirtualFiles ? QVariant() : QVariant(_selectiveSync->createBlackList()));
    wizard()->setProperty("useVirtualFiles", QVariant(useVirtualFiles));
    return true;
}

void FolderWizardSelectiveSync::cleanupPage()
{
    QString targetPath = wizard()->property("targetPath").toString();
    QString alias = QFileInfo(targetPath).fileName();
    if (alias.isEmpty())
        alias = Theme::instance()->appName();
    _selectiveSync->setFolderInfo(targetPath, alias);
    QWizardPage::cleanupPage();
}

void FolderWizardSelectiveSync::virtualFilesCheckboxClicked()
{
    // The click has already had an effect on the box, so if it's
    // checked it was newly activated.
    if (_virtualFilesCheckBox->isChecked()) {
        OwncloudWizard::askExperimentalVirtualFilesFeature(this, [this](bool enable) {
            if (!enable)
                _virtualFilesCheckBox->setChecked(false);
        });
    }
}


// ====================================================================================


/**
 * Folder wizard itself
 */

FolderWizard::FolderWizard(AccountPtr account, QWidget *parent)
    : QWizard(parent)
    , _folderWizardSourcePage(new FolderWizardLocalPath(account))
    , _folderWizardSelectiveSyncPage(new FolderWizardSelectiveSync(account))
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setPage(Page_Source, _folderWizardSourcePage);
    _folderWizardSourcePage->installEventFilter(this);
    if (!Theme::instance()->singleSyncFolder()) {
        _folderWizardTargetPage = new FolderWizardRemotePath(account);
        setPage(Page_Target, _folderWizardTargetPage);
        _folderWizardTargetPage->installEventFilter(this);
    }
    setPage(Page_SelectiveSync, _folderWizardSelectiveSyncPage);

    setWindowTitle(tr("Add Folder Sync Connection"));
    setButtonLayout({ QWizard::Stretch, QWizard::CancelButton, QWizard::NextButton, QWizard::FinishButton });
    setOptions(QWizard::CancelButtonOnLeft);
    setButtonText(QWizard::FinishButton, tr("Add Sync Connection"));
    button(QWizard::NextButton)->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));

    adjustWizardSize();
    setWizardStyle(QWizard::ClassicStyle);
    customizeStyle();
}

FolderWizard::~FolderWizard() = default;

bool FolderWizard::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest) {
        // Workaround QTBUG-3396:  forces QWizardPrivate::updateLayout()
        QTimer::singleShot(0, this, [this] { setTitleFormat(titleFormat()); });
    }
    return QWizard::eventFilter(watched, event);
}

void FolderWizard::resizeEvent(QResizeEvent *event)
{
    QWizard::resizeEvent(event);

    // workaround for QTBUG-22819: when the error label word wrap, the minimum height is not adjusted
    if (auto page = currentPage()) {
        int hfw = page->heightForWidth(page->width());
        if (page->height() < hfw) {
            page->setMinimumSize(page->minimumSizeHint().width(), hfw);
            setTitleFormat(titleFormat()); // And another workaround for QTBUG-3396
        }
    }
}

void FolderWizard::customizeStyle()
{
    // HINT: Customize wizard's own style here, if necessary in the future (Dark-/Light-Mode switching)

    // Set background colors
    auto wizardPalette = palette();
    const auto backgroundColor = QColor(IonosTheme::dialogBackgroundColor());

    // Set Color of upper part
    wizardPalette.setColor(QPalette::Base, backgroundColor);

    // Set Color of lower part
    wizardPalette.setColor(QPalette::Window, backgroundColor);

    // Set separator color
    wizardPalette.setColor(QPalette::Mid, backgroundColor);

    setPalette(wizardPalette);
}

void FolderWizard::adjustWizardSize()
{
    setFixedSize(QSize(576, 704));
}

} // end namespace
