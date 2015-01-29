#include "sharedialog.h"
#include "ui_sharedialog.h"
#include "networkjobs.h"
#include "account.h"
#include "json.h"
#include "folderman.h"
#include "folder.h"
#include "theme.h"
#include "syncresult.h"

#include "QProgressIndicator.h"
#include <QBuffer>
#include <QFileIconProvider>

namespace {
    int SHARETYPE_PUBLIC = 3;
}

namespace OCC {

ShareDialog::ShareDialog(AccountPtr account, const QString &sharePath, const QString &localPath, QWidget *parent) :
   QDialog(parent),
    _ui(new Ui::ShareDialog),
    _account(account),
    _sharePath(sharePath),
    _localPath(localPath)
{
    setAttribute(Qt::WA_DeleteOnClose);
    _ui->setupUi(this);
    _ui->pushButton_copy->setIcon(QIcon::fromTheme("edit-copy"));

    // the following progress indicator widgets are added to layouts which makes them
    // automatically deleted once the dialog dies.
    _pi_link     = new QProgressIndicator();
    _pi_password = new QProgressIndicator();
    _pi_date     = new QProgressIndicator();
    _ui->horizontalLayout_shareLink->addWidget(_pi_link);
    _ui->horizontalLayout_password->addWidget(_pi_password);
    _ui->horizontalLayout_expire->addWidget(_pi_date);

    connect(_ui->checkBox_shareLink, SIGNAL(clicked()), this, SLOT(slotCheckBoxShareLinkClicked()));
    connect(_ui->checkBox_password, SIGNAL(clicked()), this, SLOT(slotCheckBoxPasswordClicked()));
    connect(_ui->lineEdit_password, SIGNAL(returnPressed()), this, SLOT(slotPasswordReturnPressed()));
    connect(_ui->checkBox_expire, SIGNAL(clicked()), this, SLOT(slotCheckBoxExpireClicked()));
    connect(_ui->calendar, SIGNAL(clicked(QDate)), SLOT(slotCalendarClicked(QDate)));

    _ui->widget_shareLink->hide();
    _ui->lineEdit_password->hide();
    _ui->calendar->hide();

    QFileInfo f_info(_localPath);
    QFileIconProvider icon_provider;
    QIcon icon = icon_provider.icon(f_info);
    _ui->label_icon->setPixmap(icon.pixmap(40,40));

    QString name;
    if( f_info.isDir() ) {
        name = QString("Share directory %2").arg(_localPath);
    } else {
        name = QString("Share file %1").arg(_localPath);
    }
    _ui->label_name->setText(name);
    _ui->label_sharePath->setWordWrap(true);
    _ui->label_sharePath->setText(tr("%1 path: %2").arg(Theme::instance()->appNameGUI()).arg(_sharePath));
    this->setWindowTitle(tr("%1 Sharing").arg(Theme::instance()->appNameGUI()));

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

void ShareDialog::setExpireDate(const QDate &date)
{
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
    connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotExpireSet(QString)));
    job->start();
}

void ShareDialog::slotExpireSet(const QString &reply)
{
    QString message;
    int code = checkJsonReturnCode(reply, message);

    qDebug() << Q_FUNC_INFO << "Status code: " << code;
    if (code != 100) {
        displayError(code);
    } 

    _pi_date->stopAnimation();
}

void ShareDialog::slotCalendarClicked(const QDate &date)
{
    ShareDialog::setExpireDate(date);
}

ShareDialog::~ShareDialog()
{
    delete _ui;
}

void ShareDialog::slotPasswordReturnPressed()
{
    ShareDialog::setPassword(_ui->lineEdit_password->text());
    _ui->lineEdit_password->setText(QString());
    _ui->lineEdit_password->setPlaceholderText(tr("Password Protected"));
    _ui->lineEdit_password->clearFocus();
}

void ShareDialog::setPassword(const QString &password)
{
    _pi_password->startAnimation();
    QUrl url = Account::concatUrlPath(_account->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/%1").arg(_public_share_id));
    QList<QPair<QString, QString> > postParams;
    postParams.append(qMakePair(QString::fromLatin1("password"), password));
    OcsShareJob *job = new OcsShareJob("PUT", url, _account, this);
    job->setPostParams(postParams);
    connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotPasswordSet(QString)));
    job->start();
}

void ShareDialog::slotPasswordSet(const QString &reply)
{
    QString message;
    int code = checkJsonReturnCode(reply, message);

    qDebug() << Q_FUNC_INFO << "Status code: " << code;

    if (code != 100) {
        displayError(code);
    } else {
        /*
         * When setting/deleting a password from a share the old share is
         * deleted and a new one is created. So we need to refetch the shares
         * at this point.
         */
        getShares();
    }

    _pi_password->stopAnimation();
}

void ShareDialog::getShares()
{
    QUrl url = Account::concatUrlPath(_account->url(), QLatin1String("ocs/v1.php/apps/files_sharing/api/v1/shares"));
    QList<QPair<QString, QString> > params;
    params.append(qMakePair(QString::fromLatin1("path"), _sharePath));
    url.setQueryItems(params);
    OcsShareJob *job = new OcsShareJob("GET", url, _account, this);
    connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotSharesFetched(QString)));
    job->start();
}

void ShareDialog::slotSharesFetched(const QString &reply)
{
    QString message;
    int code = checkJsonReturnCode(reply, message);

    qDebug() << Q_FUNC_INFO << "Status code: " << code;
    if (code != 100 && code != 404) {
        displayError(code);
    }

    bool success = false;
    QVariantMap json = QtJson::parse(reply, success).toMap();
    ShareDialog::_shares = json.value("ocs").toMap().values("data")[0].toList();
    Q_FOREACH(auto share, ShareDialog::_shares)
    {
        QVariantMap data = share.toMap();

        if (data.value("share_type").toInt() == SHARETYPE_PUBLIC)
        {
            _public_share_id = data.value("id").toULongLong();

            _ui->widget_shareLink->show();
            _ui->checkBox_shareLink->setChecked(true);

            if (data.value("share_with").isValid())
            {
                _ui->checkBox_password->setChecked(true);
                _ui->lineEdit_password->setPlaceholderText("********");
                _ui->lineEdit_password->show();
            }

            if (data.value("expiration").isValid())
            {
                _ui->calendar->setSelectedDate(QDate::fromString(data.value("expiration").toString(), "yyyy-MM-dd 00:00:00"));
                _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));
                _ui->calendar->show();
                _ui->checkBox_expire->setChecked(true);
            }

            const QString url = Account::concatUrlPath(_account->url(), QString("public.php?service=files&t=%1").arg(data.value("token").toString())).toString();
            _ui->lineEdit_shareLink->setText(url);
        }
    }
}

void ShareDialog::slotDeleteShareFetched(const QString &reply)
{
    QString message;
    int code = checkJsonReturnCode(reply, message);

    qDebug() << Q_FUNC_INFO << "Status code: " << code;
    if (code != 100) {
        displayError(code);
    }

    _pi_link->stopAnimation();
    _ui->widget_shareLink->hide();
    _ui->lineEdit_password->hide();
    _ui->calendar->hide();
}

void ShareDialog::slotCheckBoxShareLinkClicked()
{
    if (_ui->checkBox_shareLink->checkState() == Qt::Checked)
    {
        _pi_link->startAnimation();
        QUrl url = Account::concatUrlPath(_account->url(), QLatin1String("ocs/v1.php/apps/files_sharing/api/v1/shares"));
        QList<QPair<QString, QString> > postParams;
        postParams.append(qMakePair(QString::fromLatin1("path"), _sharePath));
        postParams.append(qMakePair(QString::fromLatin1("shareType"), QString::number(SHARETYPE_PUBLIC)));
        OcsShareJob *job = new OcsShareJob("POST", url, _account, this);
        job->setPostParams(postParams);
        connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotCreateShareFetched(QString)));
        job->start();
    }
    else
    {
        _pi_link->startAnimation();
        QUrl url = Account::concatUrlPath(_account->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/%1").arg(_public_share_id));
        OcsShareJob *job = new OcsShareJob("DELETE", url, _account, this);
        connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotDeleteShareFetched(QString)));
        job->start();
    }
}

void ShareDialog::slotCreateShareFetched(const QString &reply)
{
    QString message;
    int code = checkJsonReturnCode(reply, message);

    if (code != 100) {
        displayError(code);
        return;
    }

    _pi_link->stopAnimation();
    bool success;
    QVariantMap json = QtJson::parse(reply, success).toMap();
    _public_share_id = json.value("ocs").toMap().values("data")[0].toMap().value("id").toULongLong();
    QString url = json.value("ocs").toMap().values("data")[0].toMap().value("url").toString();
    _ui->lineEdit_shareLink->setText(url);

    _ui->widget_shareLink->show();
}

void ShareDialog::slotCheckBoxPasswordClicked()
{
    if (_ui->checkBox_password->checkState() == Qt::Checked)
    {
        _ui->lineEdit_password->show();
        _ui->lineEdit_password->setPlaceholderText(tr("Choose a password for the public link"));
    }
    else
    {
        ShareDialog::setPassword(QString());
        _ui->lineEdit_password->setPlaceholderText(QString());
        _pi_password->startAnimation();
        _ui->lineEdit_password->hide();
    }
}

void ShareDialog::slotCheckBoxExpireClicked()
{
    if (_ui->checkBox_expire->checkState() == Qt::Checked)
    {
        const QDate date = QDate::currentDate().addDays(1);
        ShareDialog::setExpireDate(date);
        _ui->calendar->setSelectedDate(date);
        _ui->calendar->setMinimumDate(date);
        _ui->calendar->show();
    }
    else
    {
        ShareDialog::setExpireDate(QDate());
        _ui->calendar->hide();
    }
}

int ShareDialog::checkJsonReturnCode(const QString &reply, QString &message)
{
    bool success;
    QVariantMap json = QtJson::parse(reply, success).toMap();

    if (!success) {
        qDebug() << Q_FUNC_INFO << "Failed to parse reply";
    }

    //TODO proper checking
    int code = json.value("ocs").toMap().value("meta").toMap().value("statuscode").toInt();
    message = json.value("ocs").toMap().value("meta").toMap().value("message").toString();

    return code;
}

void ShareDialog::displayError(int code)
{
    const QString errMsg = tr("OCS API error code: %1").arg(code);
    _ui->errorLabel->setText( errMsg );
    _ui->errorLabel->show();
}

void ShareDialog::displayInfo( const QString& msg )
{
    _ui->label_sharePath->setText(msg);
}

#if 0
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
        displayInfo(tr("Can not find an folder to upload to."));
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
                                         "The file can not be registered to sync."));
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
            displayInfo(tr("The file can not be synced."));
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

OcsShareJob::OcsShareJob(const QByteArray &verb, const QUrl &url, AccountPtr account, QObject* parent)
: AbstractNetworkJob(account, "", parent),
  _verb(verb),
  _url(url)
{
    setIgnoreCredentialFailure(true);
}

void OcsShareJob::setPostParams(const QList<QPair<QString, QString> >& postParams)
{
    _postParams = postParams;
}

void OcsShareJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    QBuffer *buffer = new QBuffer;

    QStringList tmp;
    Q_FOREACH(auto tmp2, _postParams) {
        tmp.append(tmp2.first + "=" + tmp2.second);
    }
    buffer->setData(tmp.join("&").toAscii());

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
    emit jobFinished(reply()->readAll());
    return true;
}

}
