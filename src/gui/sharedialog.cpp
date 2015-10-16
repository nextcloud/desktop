/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 * Copyright (C) 2015 by Klaas Freitag <freitag@owncloud.com>
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

#include "sharedialog.h"
#include "ui_sharedialog.h"
#include "account.h"
#include "json.h"
#include "folderman.h"
#include "folder.h"
#include "accountmanager.h"
#include "theme.h"
#include "configfile.h"
#include "capabilities.h"

#include "ocssharejob.h"
#include "thumbnailjob.h"

#include "QProgressIndicator.h"
#include <QBuffer>
#include <QFileIconProvider>
#include <QClipboard>
#include <QFileInfo>

namespace OCC {

ShareDialog::ShareDialog(AccountPtr account, const QString &sharePath, const QString &localPath, bool resharingAllowed, QWidget *parent) :
   QDialog(parent),
    _ui(new Ui::ShareDialog),
    _account(account),
    _sharePath(sharePath),
    _localPath(localPath),
    _passwordJobRunning(false),
    _public_share_id(0),
    _resharingAllowed(resharingAllowed)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("SharingDialog"); // required as group for saveGeometry call

    _ui->setupUi(this);

    //Is this a file or folder?
    _isFile = QFileInfo(localPath).isFile();

    _ui->pushButton_copy->setIcon(QIcon::fromTheme("edit-copy"));
    _ui->pushButton_copy->setEnabled(false);
    connect(_ui->pushButton_copy, SIGNAL(clicked(bool)), SLOT(slotPushButtonCopyLinkPressed()));

    QPushButton *closeButton = _ui->buttonBox->button(QDialogButtonBox::Close);
    if( closeButton ) {
        connect( closeButton, SIGNAL(clicked()), this, SLOT(close()) );
    }

    // the following progress indicator widgets are added to layouts which makes them
    // automatically deleted once the dialog dies.
    _pi_link     = new QProgressIndicator();
    _pi_password = new QProgressIndicator();
    _pi_date     = new QProgressIndicator();
    _pi_editing  = new QProgressIndicator();
    _ui->horizontalLayout_shareLink->addWidget(_pi_link);
    _ui->horizontalLayout_password->addWidget(_pi_password);
    _ui->horizontalLayout_editing->addWidget(_pi_editing);
    // _ui->horizontalLayout_expire->addWidget(_pi_date);

    connect(_ui->checkBox_shareLink, SIGNAL(clicked()), this, SLOT(slotCheckBoxShareLinkClicked()));
    connect(_ui->checkBox_password, SIGNAL(clicked()), this, SLOT(slotCheckBoxPasswordClicked()));
    connect(_ui->lineEdit_password, SIGNAL(returnPressed()), this, SLOT(slotPasswordReturnPressed()));
    connect(_ui->lineEdit_password, SIGNAL(textChanged(QString)), this, SLOT(slotPasswordChanged(QString)));
    connect(_ui->pushButton_setPassword, SIGNAL(clicked(bool)), SLOT(slotPasswordReturnPressed()));
    connect(_ui->checkBox_expire, SIGNAL(clicked()), this, SLOT(slotCheckBoxExpireClicked()));
    connect(_ui->calendar, SIGNAL(dateChanged(QDate)), SLOT(slotCalendarClicked(QDate)));
    connect(_ui->checkBox_editing, SIGNAL(clicked()), this, SLOT(slotCheckBoxEditingClicked()));

    //Disable checkbox
    _ui->checkBox_shareLink->setEnabled(false);
    _pi_link->startAnimation();

    _ui->pushButton_setPassword->setEnabled(false);
    _ui->widget_shareLink->hide();
    _ui->lineEdit_password->hide();
    _ui->pushButton_setPassword->hide();

    _ui->calendar->setDate(QDate::currentDate().addDays(1));
    _ui->calendar->setEnabled(false);

    QFileInfo f_info(_localPath);
    QFileIconProvider icon_provider;
    QIcon icon = icon_provider.icon(f_info);
    _ui->label_icon->setPixmap(icon.pixmap(40,40));

    QFileInfo lPath(_localPath);
    QString fileName = lPath.fileName();
    _ui->label_name->setText(tr("%1").arg(fileName));
    QFont f( _ui->label_name->font());
    f.setPointSize( f.pointSize() * 1.4 );
    _ui->label_name->setFont( f );

    _ui->label_sharePath->setWordWrap(true);
    QString ocDir(_sharePath);
    ocDir.truncate(ocDir.length()-fileName.length());

    ocDir.replace(QRegExp("^/*"), "");
    ocDir.replace(QRegExp("/*$"), "");
    if( ocDir.isEmpty() ) {
        _ui->label_sharePath->setText(QString());
    } else {
        _ui->label_sharePath->setText(tr("Folder: %2").arg(ocDir));
    }

    this->setWindowTitle(tr("%1 Sharing").arg(Theme::instance()->appNameGUI()));
    _ui->checkBox_password->setText(tr("P&assword protect"));
    // check if the file is already inside of a synced folder
    if( sharePath.isEmpty() ) {
        // The file is not yet in an ownCloud synced folder. We could automatically
        // copy it over, but that is skipped as not all questions can be answered that
        // are involved in that, see https://github.com/owncloud/client/issues/2732
        //
        // _ui->checkBox_shareLink->setEnabled(false);
        // uploadExternalFile();
        qDebug() << Q_FUNC_INFO << "Unable to share files not in a sync folder.";
        return;
    }

    // error label, red box and stuff
    _ui->errorLabel->setLineWidth(1);
    _ui->errorLabel->setFrameStyle(QFrame::Plain);

    QPalette errPalette = _ui->errorLabel->palette();
    errPalette.setColor(QPalette::Active, QPalette::Base, QColor(0xaa, 0x4d, 0x4d));
    errPalette.setColor(QPalette::Active, QPalette::WindowText, QColor(0xaa, 0xaa, 0xaa));

    _ui->errorLabel->setPalette(errPalette);
    _ui->errorLabel->setFrameShape(QFrame::Box);
    _ui->errorLabel->setContentsMargins(QMargins(12,12,12,12));
    _ui->errorLabel->hide();


    // Parse capabilities

    // If password is enforced then don't allow users to disable it
    if (_account->capabilities().sharePublicLinkEnforcePassword()) {
        _ui->checkBox_password->setEnabled(false);
    }

    // If expiredate is enforced do not allow disable and set max days
    if (_account->capabilities().sharePublicLinkEnforceExpireDate()) {
        _ui->checkBox_expire->setEnabled(false);
        _ui->calendar->setMaximumDate(QDate::currentDate().addDays(
            _account->capabilities().sharePublicLinkExpireDateDays()
            ));
    }

    // File can't have public upload set.
    if (_isFile) {
        _ui->checkBox_editing->setEnabled(false);
    } else {
        if (!_account->capabilities().sharePublicLinkAllowUpload()) {
            _ui->checkBox_editing->setEnabled(false);
        }
    }
}

void ShareDialog::done( int r ) {
    ConfigFile cfg;
    cfg.saveGeometry(this);
    QDialog::done(r);
}

void ShareDialog::setExpireDate(const QDate &date)
{
    if( _public_share_id == 0 ) {
        // no public share so far.
        return;
    }
    _pi_date->startAnimation();

    OcsShareJob *job = new OcsShareJob(_public_share_id, _account, this);
    connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotExpireSet(QVariantMap)));
    job->setExpireDate(date);
}

void ShareDialog::slotExpireSet(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code != 100) {
        displayError(code);
    } 

    _pi_date->stopAnimation();
}

void ShareDialog::slotCalendarClicked(const QDate &date)
{
    setExpireDate(date);
}

ShareDialog::~ShareDialog()
{
    delete _ui;
}

void ShareDialog::slotPasswordReturnPressed()
{
    setPassword(_ui->lineEdit_password->text());
    _ui->lineEdit_password->setText(QString());
    _ui->lineEdit_password->setPlaceholderText(tr("Password Protected"));
    _ui->lineEdit_password->clearFocus();
}

void ShareDialog::slotPasswordChanged(const QString& newText)
{
    // disable the set-password button
    _ui->pushButton_setPassword->setEnabled( newText.length() > 0 );
}

void ShareDialog::setPassword(const QString &password)
{
    if( _passwordJobRunning ) {
        // This happens because the entry field and the button both trigger this slot.
        return;
    }
    _pi_link->startAnimation();
    _pi_password->startAnimation();
    QString path;

    if( _public_share_id > 0 ) {
        OcsShareJob *job = new OcsShareJob(_public_share_id, _account, this);
        connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotPasswordSet(QVariantMap)));
        job->setPassword(password);
    } else {
        OcsShareJob *job = new OcsShareJob(_public_share_id, _account, this);
        connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotPasswordSet(QVariantMap)));
        connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotCreateShareFetched(QVariantMap)));

        QDate date;
        if( _ui->checkBox_expire->isChecked() ) {
            date = _ui->calendar->date();
        }

        job->createShare(_sharePath, OcsShareJob::SHARETYPE::LINK, password, date);
    }
    _passwordJobRunning = true;
}

void ShareDialog::slotPasswordSet(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code != 100) {
        displayError(code);
    }
    /*
         * When setting/deleting a password from a share the old share is
         * deleted and a new one is created. So we need to refetch the shares
         * at this point.
         */
    getShares();

    _passwordJobRunning = false;
    _pi_password->stopAnimation();
}

void ShareDialog::getShares()
{
    OcsShareJob *job = new OcsShareJob(_account, this);
    connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotSharesFetched(QVariantMap)));
    job->getShares(_sharePath);

    if (QFileInfo(_localPath).isFile()) {
        ThumbnailJob *job2 = new ThumbnailJob(_sharePath, _account, this);
        connect(job2, SIGNAL(jobFinished(int, QByteArray)), SLOT(slotThumbnailFetched(int, QByteArray)));
        job2->start();
    }
}

void ShareDialog::slotSharesFetched(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code != 100 && code != 404) {
        displayError(code);
    }

    ShareDialog::_shares = reply.value("ocs").toMap().value("data").toList();
    const QString versionString = _account->serverVersion();

    qDebug() << Q_FUNC_INFO << versionString << "Fetched" << ShareDialog::_shares.count() << "shares";

    //Show link checkbox now
    _ui->checkBox_shareLink->setEnabled(true);
    _pi_link->stopAnimation();

    Q_FOREACH(auto share, ShareDialog::_shares) {
        QVariantMap data = share.toMap();

        if (data.value("share_type").toInt() == static_cast<int>(OcsShareJob::SHARETYPE::LINK)) {
            _public_share_id = data.value("id").toULongLong();
            _ui->pushButton_copy->show();

            _ui->widget_shareLink->show();
            _ui->checkBox_shareLink->setChecked(true);

            if (data.value("share_with").isValid()) {
                _ui->checkBox_password->setChecked(true);
                _ui->lineEdit_password->setPlaceholderText("********");
                _ui->lineEdit_password->show();
                _ui->pushButton_setPassword->show();
            } else {
                _ui->checkBox_password->setChecked(false);
                // _ui->lineEdit_password->setPlaceholderText("********");
                _ui->lineEdit_password->hide();
                _ui->pushButton_setPassword->hide();
            }

            if (data.value("expiration").isValid()) {
                _ui->calendar->setDate(QDate::fromString(data.value("expiration").toString(), "yyyy-MM-dd 00:00:00"));
                _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));
                _ui->calendar->setEnabled(true);
                _ui->checkBox_expire->setChecked(true);
            } else {
                _ui->calendar->setEnabled(false);
                _ui->checkBox_expire->setChecked(false);
            }

            if (data.value("permissions").isValid()) {
                int permissions = data.value("permissions").toInt();
                /*
                 * Only directories can have public upload set
                 * For public links the server sets CREATE and UPDATE permissions.
                 */
                if (!_isFile && 
                       (permissions & static_cast<int>(OcsShareJob::PERMISSION::UPDATE)) && 
                       (permissions & static_cast<int>(OcsShareJob::PERMISSION::CREATE))) {
                    _ui->checkBox_editing->setChecked(true);
                }
            }

            QString url;
            // From ownCloud server 8.2 the url field is always set for public shares
            if (data.contains("url")) {
                url = data.value("url").toString();
            } else if (versionString.contains('.') && versionString.split('.')[0].toInt() >= 8) {
                // From ownCloud server version 8 on, a different share link scheme is used.
                url = Account::concatUrlPath(_account->url(), QString("index.php/s/%1").arg(data.value("token").toString())).toString();
            } else {
                QList<QPair<QString, QString>> queryArgs;
                queryArgs.append(qMakePair(QString("service"), QString("files")));
                queryArgs.append(qMakePair(QString("t"), data.value("token").toString()));
                url = Account::concatUrlPath(_account->url(), QLatin1String("public.php"), queryArgs).toString();
            }
            setShareLink(url);

            _ui->pushButton_copy->setEnabled(true);
        }
    }
    if( _shares.count()>0 ) {
        setShareCheckBoxTitle(true);
    } else {
        // If there are no shares yet, check the checkbox to create a link automatically.
        // If its clear that resharing is not allowed, display an error
        if( !_resharingAllowed ) {
            displayError(tr("The file can not be shared because it was shared without sharing permission."));
            _ui->checkBox_shareLink->setEnabled(false);
        } else {
            _ui->checkBox_shareLink->setChecked(true);
            slotCheckBoxShareLinkClicked();
        }
    }
}

void ShareDialog::resizeEvent(QResizeEvent *e)
{
    QDialog::resizeEvent(e);
    redrawElidedUrl();
}

void ShareDialog::redrawElidedUrl()
{
    QString u;

    if( !_shareUrl.isEmpty() ) {
        QFontMetrics fm( _ui->_labelShareLink->font() );
        int linkLengthPixel = _ui->_labelShareLink->width();

        const QUrl realUrl(_shareUrl);
        QString elidedUrl = fm.elidedText(_shareUrl, Qt::ElideRight, linkLengthPixel);

        u = QString("<a href=\"%1\">%2</a>").arg(realUrl.toString(QUrl::None)).arg(elidedUrl);
    }
    _ui->_labelShareLink->setText(u);
}

void ShareDialog::setShareLink( const QString& url )
{
    // FIXME: shorten the url for output.
    const QUrl realUrl(url);
    if( realUrl.isValid() ) {
        _shareUrl = url;
        _ui->pushButton_copy->setEnabled(true);
    } else {
        _shareUrl.clear();
        _ui->_labelShareLink->setText(QString::null);
    }
    redrawElidedUrl();

}

void ShareDialog::slotDeleteShareFetched(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code != 100) {
        displayError(code);
    }

    _public_share_id = 0;
    _pi_link->stopAnimation();
    _ui->lineEdit_password->clear();
    _ui->_labelShareLink->clear();
    _ui->pushButton_copy->setEnabled(false);
    _ui->widget_shareLink->hide();
    _ui->lineEdit_password->hide();
    _ui->pushButton_setPassword->setEnabled(false);
    _ui->pushButton_setPassword->hide();
    _ui->checkBox_expire->setChecked(false);
    _ui->checkBox_password->setChecked(false);
    _ui->calendar->setEnabled(false);

    _shareUrl.clear();

    setShareCheckBoxTitle(false);

}

void ShareDialog::slotCheckBoxShareLinkClicked()
{
    qDebug() << Q_FUNC_INFO <<( _ui->checkBox_shareLink->checkState() == Qt::Checked);
    if (_ui->checkBox_shareLink->checkState() == Qt::Checked) {
        _pi_link->startAnimation();

        /*
         * Check the capabilities if the server requires a password for a share
         * Ask for it directly
         */
        if (_account->capabilities().sharePublicLinkEnforcePassword()) {
            _pi_link->stopAnimation();
            _ui->checkBox_password->setChecked(true);
            _ui->checkBox_password->setEnabled(false);
            _ui->checkBox_password->setText(tr("Public sh&aring requires a password"));
            _ui->lineEdit_password->setFocus();
            _ui->pushButton_copy->hide();
            _ui->widget_shareLink->show();

            slotCheckBoxPasswordClicked();
            return;
        }

        OcsShareJob *job = new OcsShareJob(_account, this);
        connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotCreateShareFetched(QVariantMap)));
        job->createShare(_sharePath, OcsShareJob::SHARETYPE::LINK);
    } else {
        _pi_link->startAnimation();
        OcsShareJob *job = new OcsShareJob(_public_share_id, _account, this);
        connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotDeleteShareFetched(QVariantMap)));
        job->deleteShare();
    }
}

void ShareDialog::slotCreateShareFetched(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    _pi_link->stopAnimation();

    if (code == 403) {
        // there needs to be a password
        _ui->checkBox_password->setChecked(true);
        _ui->checkBox_password->setEnabled(false);
        _ui->checkBox_password->setText(tr("Public sh&aring requires a password"));
        _ui->lineEdit_password->setFocus();
        _ui->pushButton_copy->hide();
        _ui->widget_shareLink->show();

        slotCheckBoxPasswordClicked();
        return;
    } else if (code != 100) {
        displayError(code);
        return;
    }

    _public_share_id = reply.value("ocs").toMap().values("data")[0].toMap().value("id").toULongLong();
    _ui->pushButton_copy->show();
    getShares();
}

void ShareDialog::slotCheckBoxPasswordClicked()
{
    if (_ui->checkBox_password->checkState() == Qt::Checked) {
        _ui->lineEdit_password->show();
        _ui->pushButton_setPassword->show();
        _ui->lineEdit_password->setPlaceholderText(tr("Please Set Password"));
        _ui->lineEdit_password->setFocus();
    } else {
        ShareDialog::setPassword(QString());
        _ui->lineEdit_password->setPlaceholderText(QString());
        _pi_password->startAnimation();
        _ui->lineEdit_password->hide();
        _ui->pushButton_setPassword->hide();
    }
}

void ShareDialog::slotCheckBoxExpireClicked()
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
        ShareDialog::setExpireDate(QDate());
        _ui->calendar->setEnabled(false);
    }
}

void ShareDialog::slotPushButtonCopyLinkPressed()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(_shareUrl);
}

void ShareDialog::slotCheckBoxEditingClicked()
{
    ShareDialog::setPublicUpload(_ui->checkBox_editing->checkState() == Qt::Checked);
}

void ShareDialog::setPublicUpload(bool publicUpload)
{
    _ui->checkBox_editing->setEnabled(false);
    _pi_editing->startAnimation();

    OcsShareJob *job = new OcsShareJob(_public_share_id, _account, this);
    connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotPublicUploadSet(QVariantMap)));
    job->setPublicUpload(publicUpload);
}

void ShareDialog::slotPublicUploadSet(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);
    if (code == 100) {
        _ui->checkBox_editing->setEnabled(true);
    } else {
        qDebug() << Q_FUNC_INFO << reply;
        displayError(code);
    }

    _pi_editing->stopAnimation();
}

void ShareDialog::setShareCheckBoxTitle(bool haveShares)
{
    const QString noSharesTitle(tr("&Share link"));
    const QString haveSharesTitle(tr("&Share link"));

    if( haveShares ) {
        _ui->checkBox_shareLink->setText( haveSharesTitle );
    } else {
        _ui->checkBox_shareLink->setText( noSharesTitle );
    }

}

void ShareDialog::displayError(const QString& errMsg)
{
    _ui->errorLabel->setText( errMsg );
    _ui->errorLabel->show();
}

void ShareDialog::displayError(int code)
{
    const QString errMsg = tr("OCS API error code: %1").arg(code);
    displayError(errMsg);
}

#if 0
void ShareDialog::displayInfo( const QString& msg )
{
    _ui->label_sharePath->setText(msg);
}

/*
 * This code is disabled for now as we do not have answers for all the questions involved
 * here, see https://github.com/owncloud/client/issues/2732
 */
bool ShareDialog::uploadExternalFile()
{
    bool re = false;
    const QString folderName = QString("ownCloud"); // FIXME: get a proper folder name

    Folder *folder = 0;
    Folder::Map folders = FolderMan::instance()->map();
    if( folders.isEmpty() ) {
        displayInfo(tr("There is no sync folder configured."));
        return false;
    }
    if( folders.contains( Theme::instance()->appNameGUI()) ) {
        folder = folders.value(Theme::instance()->appNameGUI());
    }
    if( !folder ) {
        folder = folders.value( folders.keys().at(0));
    }
    FolderMan::instance()->folder(folderName);
    if( ! folder ) {
        qDebug() << "Folder not defined: " << folderName;
        displayInfo(tr("Cannot find a folder to upload to."));
        return false;
    }

    QFileInfo fi(_localPath);
    if( fi.isDir() ) {
        // we can not do this for directories yet.
        displayInfo(tr("Sharing of external directories is not yet working."));
        return false;
    }
    _sharePath = folder->remotePath()+QLatin1Char('/')+fi.fileName();
    _folderAlias = folderName;

    // connect the finish signal of the folder before the file to upload
    // is copied to the sync folder.
    connect( folder, SIGNAL(syncFinished(SyncResult)), this, SLOT(slotNextSyncFinished(SyncResult)) );

    // copy the file
    _expectedSyncFile = folder->path()+fi.fileName();

    QFileInfo target(_expectedSyncFile);
    if( target.exists() ) {
        _ui->label_sharePath->setText(tr("A sync file with the same name exists. "
                                         "The file cannot be registered to sync."));
        // TODO: Add a file comparison here. If the existing file is still the same
        // as the file-to-copy we can share it.
        _sharePath.clear();
    } else {
        _uploadFails = 0;
        _ui->pi_share->startAnimation();
        QFile file( _localPath);
        if( file.copy(_expectedSyncFile) ) {
            // copying succeeded.
            re = true;
            displayInfo(tr("Waiting to upload..."));
        } else {
            displayInfo(tr("Unable to register in sync space."));
        }
    }
    return re;
}

void ShareDialog::slotNextSyncFinished( const SyncResult& result )
{
    // FIXME: Check for state!
    SyncFileItemVector itemVector = result.syncFileItemVector();
    SyncFileItem targetItem;
    Folder *folder = FolderMan::instance()->folder(_folderAlias);
    const QString folderPath = folder->path();

    _ui->pi_share->stopAnimation();

    foreach( SyncFileItem item, itemVector ) {
        const QString fullSyncedFile = folderPath + item._file;
        if( item._direction == SyncFileItem::Up &&
                fullSyncedFile == _expectedSyncFile) {
            // found the item!
            targetItem = item;
            continue;
        }
    }

    if( targetItem.isEmpty() ) {
        // The item was not in this sync run. Lets wait for the next one. FIXME
        _uploadFails ++;
        if( _uploadFails > 2 ) {
            // stop the upload job
            displayInfo(tr("The file cannot be synced."));
        }
    } else {
        // it's there and the sync was successful.
        // The server should be able to generate a share link now.
        // Enable the sharing link
        if( targetItem._status == SyncFileItem::Success ) {
            _ui->checkBox_shareLink->setEnabled(true);
            _ui->label_sharePath->setText(tr("%1 path: %2").arg(Theme::instance()->appNameGUI()).arg(_sharePath));
        } else {
            displayInfo(tr("Sync of registered file was not successful yet."));
        }
    }
    _expectedSyncFile.clear();
}
#endif

void ShareDialog::slotThumbnailFetched(const int &statusCode, const QByteArray &reply)
{
    if (statusCode != 200) {
        qDebug() << Q_FUNC_INFO << "Status code: " << statusCode;
        return;
    }

    QPixmap p;
    p.loadFromData(reply, "PNG");
    _ui->label_icon->setPixmap(p);
}

}
