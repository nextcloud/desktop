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
#include "networkjobs.h"
#include "account.h"
#include "json.h"
#include "folderman.h"
#include "folder.h"
#include "accountmanager.h"
#include "theme.h"
#include "syncresult.h"
#include "configfile.h"

#include "QProgressIndicator.h"
#include <QBuffer>
#include <QFileIconProvider>
#include <QClipboard>

namespace {
    int SHARETYPE_PUBLIC = 3;
}

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
    _ui->horizontalLayout_shareLink->addWidget(_pi_link);
    _ui->horizontalLayout_password->addWidget(_pi_password);
    // _ui->horizontalLayout_expire->addWidget(_pi_date);

    connect(_ui->checkBox_shareLink, SIGNAL(clicked()), this, SLOT(slotCheckBoxShareLinkClicked()));
    connect(_ui->checkBox_password, SIGNAL(clicked()), this, SLOT(slotCheckBoxPasswordClicked()));
    connect(_ui->lineEdit_password, SIGNAL(returnPressed()), this, SLOT(slotPasswordReturnPressed()));
    connect(_ui->lineEdit_password, SIGNAL(textChanged(QString)), this, SLOT(slotPasswordChanged(QString)));
    connect(_ui->pushButton_setPassword, SIGNAL(clicked(bool)), SLOT(slotPasswordReturnPressed()));
    connect(_ui->checkBox_expire, SIGNAL(clicked()), this, SLOT(slotCheckBoxExpireClicked()));
    connect(_ui->calendar, SIGNAL(dateChanged(QDate)), SLOT(slotCalendarClicked(QDate)));

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

    if( ocDir == QLatin1String("/")) {
        _ui->label_sharePath->setText(QString());
    } else {
        if( ocDir.startsWith(QLatin1Char('/')) ) {
            ocDir = ocDir.mid(1, -1);
        }
        if( ocDir.endsWith(QLatin1Char('/')) ) {
            ocDir.chop(1);
        }
        _ui->label_sharePath->setText(tr("Folder: %2").arg(ocDir));
    }

    this->setWindowTitle(tr("%1 Sharing").arg(Theme::instance()->appNameGUI()));
    _ui->checkBox_password->setText(tr("P&assword protect"));
    // check if the file is already inside of a synced folder
    if( sharePath.isEmpty() ) {
        // The file is not yet in an ownCloud synced folder. We could automatically
        // copy it over, but that is skipped as not all questions can be anwered that
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
}

void ShareDialog::done( int r ) {
    ConfigFile cfg;
    cfg.saveGeometry(this);
    QDialog::done(r);
}

static int getJsonReturnCode(const QVariantMap &json, QString &message)
{
    //TODO proper checking
    int code = json.value("ocs").toMap().value("meta").toMap().value("statuscode").toInt();
    message = json.value("ocs").toMap().value("meta").toMap().value("message").toString();

    return code;
}

void ShareDialog::setExpireDate(const QDate &date)
{
    if( _public_share_id == 0 ) {
        // no public share so far.
        return;
    }
    _pi_date->startAnimation();
    QUrl url = Account::concatUrlPath(_account->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/%1").arg(_public_share_id));
    QList<QPair<QString, QString> > postParams;

    if (date.isValid()) {
        postParams.append(qMakePair(QString::fromLatin1("expireDate"), date.toString("yyyy-MM-dd")));
    } else {
        postParams.append(qMakePair(QString::fromLatin1("expireDate"), QString()));
    }

    OcsShareJob *job = new OcsShareJob("PUT", url, _account, this);
    job->setPostParams(postParams);
    connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotExpireSet(QVariantMap)));
    job->start();
}

void ShareDialog::slotExpireSet(const QVariantMap &reply)
{
    QString message;
    int code = getJsonReturnCode(reply, message);
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
    // disable the set-passwort button
    _ui->pushButton_setPassword->setEnabled( newText.length() > 0 );
}

void ShareDialog::setPassword(const QString &password)
{
    if( _passwordJobRunning ) {
        // This happens because the entry field and the button both trigger this slot.
        return;
    }
    _pi_password->startAnimation();
    QUrl url;
    QList<QPair<QString, QString> > requestParams;
    QByteArray verb("PUT");

    if( _public_share_id > 0 ) {
        url = Account::concatUrlPath(_account->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/%1").arg(_public_share_id));
        requestParams.append(qMakePair(QString::fromLatin1("password"), password));
    } else {
        // lets create a new share.
        url = Account::concatUrlPath(_account->url(), QLatin1String("ocs/v1.php/apps/files_sharing/api/v1/shares"));
        requestParams.append(qMakePair(QString::fromLatin1("path"), _sharePath));
        requestParams.append(qMakePair(QString::fromLatin1("shareType"), QString::number(SHARETYPE_PUBLIC)));
        requestParams.append(qMakePair(QString::fromLatin1("password"), password));
        verb = "POST";

        if( _ui->checkBox_expire->isChecked() ) {
            QDate date = _ui->calendar->date();
            if( date.isValid() ) {
                requestParams.append(qMakePair(QString::fromLatin1("expireDate"), date.toString("yyyy-MM-dd")));
            }
        }
    }
    OcsShareJob *job = new OcsShareJob(verb, url, _account, this);
    job->setPostParams(requestParams);
    connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotPasswordSet(QVariantMap)));
    job->start();
    _passwordJobRunning = true;
}

void ShareDialog::slotPasswordSet(const QVariantMap &reply)
{
    QString message;
    int code = getJsonReturnCode(reply, message);
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
    QUrl url = Account::concatUrlPath(_account->url(), QLatin1String("ocs/v1.php/apps/files_sharing/api/v1/shares"));
    QList<QPair<QString, QString> > params;
    params.append(qMakePair(QString::fromLatin1("path"), _sharePath));
    url.setQueryItems(params);
    OcsShareJob *job = new OcsShareJob("GET", url, _account, this);
    job->addPassStatusCode(404); // don't report error if share doesn't exist yet
    connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotSharesFetched(QVariantMap)));
    job->start();
}

void ShareDialog::slotSharesFetched(const QVariantMap &reply)
{
    QString message;
    int code = getJsonReturnCode(reply, message);
    if (code != 100 && code != 404) {
        displayError(code);
    }

    ShareDialog::_shares = reply.value("ocs").toMap().value("data").toList();
    const QString versionString = _account->serverVersion();

    Q_FOREACH(auto share, ShareDialog::_shares) {
        QVariantMap data = share.toMap();

        if (data.value("share_type").toInt() == SHARETYPE_PUBLIC) {
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

            QString url;
            // From ownCloud server version 8 on, a different share link scheme is used.
            if (versionString.contains('.') && versionString.split('.')[0].toInt() >= 8) {
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
    int code = getJsonReturnCode(reply, message);
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
    if (_ui->checkBox_shareLink->checkState() == Qt::Checked) {
        _pi_link->startAnimation();
        QUrl url = Account::concatUrlPath(_account->url(), QLatin1String("ocs/v1.php/apps/files_sharing/api/v1/shares"));
        QList<QPair<QString, QString> > postParams;
        postParams.append(qMakePair(QString::fromLatin1("path"), _sharePath));
        postParams.append(qMakePair(QString::fromLatin1("shareType"), QString::number(SHARETYPE_PUBLIC)));
        OcsShareJob *job = new OcsShareJob("POST", url, _account, this);
        job->setPostParams(postParams);
        job->addPassStatusCode(403); // "password required" is not an error
        connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotCreateShareFetched(QVariantMap)));
        job->start();
    } else {
        _pi_link->startAnimation();
        QUrl url = Account::concatUrlPath(_account->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/%1").arg(_public_share_id));
        OcsShareJob *job = new OcsShareJob("DELETE", url, _account, this);
        connect(job, SIGNAL(jobFinished(QVariantMap)), this, SLOT(slotDeleteShareFetched(QVariantMap)));
        job->start();
    }
}

void ShareDialog::slotCreateShareFetched(const QVariantMap &reply)
{
    QString message;
    int code = getJsonReturnCode(reply, message);
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
        _ui->lineEdit_password->setPlaceholderText(tr("Password"));
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
        ShareDialog::setExpireDate(date);
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
        // then the file-to-copy we can share it.
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

OcsShareJob::OcsShareJob(const QByteArray &verb, const QUrl &url, AccountPtr account, QObject* parent)
: AbstractNetworkJob(account, "", parent),
  _verb(verb),
  _url(url)
{
    _passStatusCodes.append(100);
    setIgnoreCredentialFailure(true);
}

void OcsShareJob::setPostParams(const QList<QPair<QString, QString> >& postParams)
{
    _postParams = postParams;
}

void OcsShareJob::addPassStatusCode(int code)
{
    _passStatusCodes.append(code);
}

void OcsShareJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setRawHeader("Content-Type", "application/x-www-form-urlencoded");

    // Url encode the _postParams and put them in a buffer.
    QByteArray postData;
    Q_FOREACH(auto tmp2, _postParams) {
        if (! postData.isEmpty()) {
            postData.append("&");
        }
        postData.append(QUrl::toPercentEncoding(tmp2.first));
        postData.append("=");
        postData.append(QUrl::toPercentEncoding(tmp2.second));
    }
    QBuffer *buffer = new QBuffer;
    buffer->setData(postData);

    auto queryItems = _url.queryItems();
    queryItems.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
    _url.setQueryItems(queryItems);

    setReply(davRequest(_verb, _url, req, buffer));
    setupConnections(reply());
    buffer->setParent(reply());
    AbstractNetworkJob::start();
}

bool OcsShareJob::finished()
{
    const QString replyData = reply()->readAll();

    bool success;
    QVariantMap json = QtJson::parse(replyData, success).toMap();
    if (!success) {
        qDebug() << "Could not parse reply to" << _verb << _url << _postParams
                 << ":" << replyData;
    }

    QString message;
    const int statusCode = getJsonReturnCode(json, message);
    if (!_passStatusCodes.contains(statusCode)) {
        qDebug() << "Reply to" << _verb << _url << _postParams
                 << "has unexpected status code:" << statusCode << replyData;
    }

    emit jobFinished(json);
    return true;
}

}
