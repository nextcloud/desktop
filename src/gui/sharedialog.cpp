#include "sharedialog.h"
#include "ui_sharedialog.h"
#include "networkjobs.h"
#include "account.h"
#include "json.h"
#include "folderman.h"
#include "QProgressIndicator.h"
#include <QBuffer>
#include <QMovie>
#include <QMessageBox>
#include <QFileIconProvider>

namespace {
    int SHARETYPE_PUBLIC = 3;
}

namespace OCC {

ShareDialog::ShareDialog(const QString &sharePath, const QString &localPath, QWidget *parent) :
   QDialog(parent),
    _ui(new Ui::ShareDialog),
    _sharePath(sharePath),
    _localPath(localPath)
{
    setAttribute(Qt::WA_DeleteOnClose);
    _ui->setupUi(this);
    _ui->pushButton_copy->setIcon(QIcon::fromTheme("edit-copy"));
    layout()->setSizeConstraint(QLayout::SetFixedSize);

    _pi_link = new QProgressIndicator();
    _pi_password = new QProgressIndicator();
    _pi_date = new QProgressIndicator();
    _pi_password->hide();
    _pi_date->hide();
    _ui->horizontalLayout_4->addWidget(_pi_link);
    _ui->horizontalLayout_5->addWidget(_pi_password);
    _ui->horizontalLayout_3->addWidget(_pi_date);

    connect(_ui->checkBox_shareLink, SIGNAL(clicked()), this, SLOT(slotCheckBoxShareLinkClicked()));
    connect(_ui->checkBox_password, SIGNAL(clicked()), this, SLOT(slotCheckBoxPasswordClicked()));
    connect(_ui->lineEdit_password, SIGNAL(returnPressed()), this, SLOT(slotPasswordReturnPressed()));
    connect(_ui->checkBox_expire, SIGNAL(clicked()), this, SLOT(slotCheckBoxExpireClicked()));
    connect(_ui->calendar, SIGNAL(clicked(QDate)), SLOT(slotCalendarClicked(QDate)));

    _ui->lineEdit_shareLink->hide();
    _ui->pushButton_copy->hide();
    _ui->lineEdit_password->hide();
    _ui->checkBox_password->hide();
    _ui->checkBox_expire->hide();
    _ui->calendar->hide();

    QFileInfo f_info(_localPath);
    QFileIconProvider icon_provider;
    QIcon icon = icon_provider.icon(f_info);
    _ui->label_icon->setPixmap(icon.pixmap(40,40));
    if (f_info.isDir()) {
        _ui->lineEdit_name->setText(f_info.dir().dirName());
        _ui->lineEdit_type->setText("Directory");
    } else {
        _ui->lineEdit_name->setText(f_info.fileName());
        _ui->lineEdit_type->setText("File");
    }
    _ui->lineEdit_localPath->setText(_localPath);
    _ui->lineEdit_sharePath->setText(_sharePath);
}

void ShareDialog::setExpireDate(const QString &date)
{
    _pi_date->startAnimation();
    QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/").append(QString::number(_public_share_id)));
    QUrl postData;
    QList<QPair<QString, QString> > getParams;
    QList<QPair<QString, QString> > postParams;
    getParams.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
    postParams.append(qMakePair(QString::fromLatin1("expireDate"), date));
    url.setQueryItems(getParams);
    postData.setQueryItems(postParams);
    OcsShareJob *job = new OcsShareJob("PUT", url, postData, AccountManager::instance()->account(), this);
    connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotExpireSet(QString)));
    job->start();
}

void ShareDialog::slotExpireSet(const QString &reply)
{
    int code = checkJsonReturnCode(reply);

    qDebug() << Q_FUNC_INFO << "Status code: " << code;
    if (code == 100) {
    } else if (code == 400) {
        QMessageBox msgBox;
        msgBox.setText("Wrong or no update parameter given");
        msgBox.exec();
    } else if (code == 403) {
        QMessageBox msgBox;
        msgBox.setText("Public upload disabled by the admin");
        msgBox.exec();
    } else if (code == 404) {
        QMessageBox msgBox;
        msgBox.setText("Couldn’t update share");
        msgBox.exec();
    } else {
        qDebug() << Q_FUNC_INFO << "Unkown status code: " << code;
    }
    _pi_date->stopAnimation();
}

void ShareDialog::slotCalendarClicked(const QDate &date)
{
    ShareDialog::setExpireDate(date.toString("yyyy-MM-dd"));
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
    QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/").append(QString::number(_public_share_id)));
    QUrl postData;
    QList<QPair<QString, QString> > getParams;
    QList<QPair<QString, QString> > postParams;
    getParams.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
    postParams.append(qMakePair(QString::fromLatin1("password"), password));
    url.setQueryItems(getParams);
    postData.setQueryItems(postParams);
    OcsShareJob *job = new OcsShareJob("PUT", url, postData, AccountManager::instance()->account(), this);
    connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotPasswordSet(QString)));
    job->start();
}

void ShareDialog::slotPasswordSet(const QString &reply)
{
    int code = checkJsonReturnCode(reply);

    qDebug() << Q_FUNC_INFO << "Status code: " << code;
    if (code == 100) {
    } else if (code == 400) {
        QMessageBox msgBox;
        msgBox.setText("Wrong or no update parameter given");
        msgBox.exec();
    } else if (code == 403) {
        QMessageBox msgBox;
        msgBox.setText("Public upload disabled by the admin");
        msgBox.exec();
    } else if (code == 404) {
        QMessageBox msgBox;
        msgBox.setText("Couldn’t update share");
        msgBox.exec();
    } else {
        qDebug() << Q_FUNC_INFO << "Unkown status code: " << code;
    }

    _pi_password->stopAnimation();
}

void ShareDialog::getShares()
{
    this->setWindowTitle(tr("Sharing %1").arg(_sharePath));
    QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QLatin1String("ocs/v1.php/apps/files_sharing/api/v1/shares"));
    QList<QPair<QString, QString> > params;
    params.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
    params.append(qMakePair(QString::fromLatin1("path"), _sharePath));
    url.setQueryItems(params);
    OcsShareJob *job = new OcsShareJob("GET", url, QUrl(), AccountManager::instance()->account(), this);
    connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotSharesFetched(QString)));
    job->start();
}

void ShareDialog::slotSharesFetched(const QString &reply)
{
    int code = checkJsonReturnCode(reply);

    qDebug() << Q_FUNC_INFO << "Status code: " << code;
    if (code == 100) {
    } else if (code == 400) {
        QMessageBox msgBox;
        msgBox.setText("Not a directory (if the ‘subfile’ argument was used)");
        msgBox.exec();
        return;
    } else if (code == 404) {
        QMessageBox msgBox;
        msgBox.setText("File doesn’t exist");
        msgBox.exec();
        return;
    } else {
        qDebug() << Q_FUNC_INFO << "Unkown status code: " << code;
        return;
    }

    bool success = false;
    QVariantMap json = QtJson::parse(reply, success).toMap();
    ShareDialog::_shares = json.value("ocs").toMap().values("data")[0].toList();
    for(int i = 0; i < ShareDialog::_shares.count(); i++)
    {
        QVariantMap data = ShareDialog::_shares[i].toMap();

        if (data.value("share_type").toInt() == SHARETYPE_PUBLIC)
        {
            _public_share_id = data.value("id").toULongLong();

            _ui->lineEdit_shareLink->show();
            _ui->pushButton_copy->show();
            _ui->checkBox_password->show();
            _ui->checkBox_expire->show();
            _ui->pushButton_copy->show();
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
                _ui->calendar->show();
                _ui->checkBox_expire->setChecked(true);
            }

            const QString url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QString("public.php?service=files&t=%1").arg(data.value("token").toString())).toString();
            _ui->lineEdit_shareLink->setText(url);
        }
    }
}

void ShareDialog::slotDeleteShareFetched(const QString & /* reply */)
{
    _pi_password->hide();
    _pi_date->hide();

    _pi_link->stopAnimation();
    _ui->lineEdit_shareLink->hide();
    _ui->pushButton_copy->hide();
    _ui->lineEdit_password->hide();
    _ui->checkBox_password->hide();
    _ui->checkBox_expire->hide();
    _ui->calendar->hide();
}

void ShareDialog::slotCheckBoxShareLinkClicked()
{
    if (_ui->checkBox_shareLink->checkState() == Qt::Checked)
    {
        _pi_link->startAnimation();
        QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QLatin1String("ocs/v1.php/apps/files_sharing/api/v1/shares"));
        QUrl postData;
        QList<QPair<QString, QString> > getParams;
        QList<QPair<QString, QString> > postParams;
        getParams.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
        postParams.append(qMakePair(QString::fromLatin1("path"), _sharePath));
        postParams.append(qMakePair(QString::fromLatin1("shareType"), QString::number(SHARETYPE_PUBLIC)));
        url.setQueryItems(getParams);
        postData.setQueryItems(postParams);
        OcsShareJob *job = new OcsShareJob("POST", url, postData, AccountManager::instance()->account(), this);
        connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotCreateShareFetched(QString)));
        job->start();
    }
    else
    {
        _pi_link->startAnimation();
        QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/").append(QString::number(_public_share_id)));
        QList<QPair<QString, QString> > getParams;
        getParams.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
        url.setQueryItems(getParams);
        OcsShareJob *job = new OcsShareJob("DELETE", url, QUrl(), AccountManager::instance()->account(), this);
        connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotDeleteShareFetched(QString)));
        job->start();
    }
}

void ShareDialog::slotCreateShareFetched(const QString &reply)
{
    int code = checkJsonReturnCode(reply);

    qDebug() << Q_FUNC_INFO << "Status code: " << code;
    if (code == 100) {
    } else if (code == 400) {
        QMessageBox msgBox;
        msgBox.setText("Unknown share type");
        msgBox.exec();
        return;
    } else if (code == 403) {
        QMessageBox msgBox;
        msgBox.setText("Public upload was disabled by the admin");
        msgBox.exec();
        return;
    } else if (code == 404) {
        QMessageBox msgBox;
        msgBox.setText("File couldn’t be shared");
        msgBox.exec();
        return;
    } else {
        qDebug() << Q_FUNC_INFO << "Unkown status code: " << code;
        return;
    }



    _pi_link->stopAnimation();
    _pi_password->show();
    _pi_date->show();
    bool success;
    QVariantMap json = QtJson::parse(reply, success).toMap();
    _public_share_id = json.value("ocs").toMap().values("data")[0].toMap().value("id").toULongLong();
    QString url = json.value("ocs").toMap().values("data")[0].toMap().value("url").toString();
    _ui->lineEdit_shareLink->setText(url);
    _ui->lineEdit_shareLink->show();
    _ui->pushButton_copy->show();
    _ui->checkBox_password->show();
    _ui->checkBox_expire->show();
    _ui->pushButton_copy->show();
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
        QDate date = QDate::currentDate().addDays(1);
        ShareDialog::setExpireDate(date.toString("dd-MM-yyyy"));
        _ui->calendar->setSelectedDate(date);
        _ui->calendar->show();
    }
    else
    {
        ShareDialog::setExpireDate(QString());
        _ui->calendar->hide();
    }
}

int ShareDialog::checkJsonReturnCode(const QString &reply)
{
    bool success;
    QVariantMap json = QtJson::parse(reply, success).toMap();

    if (!success) {
        qDebug() << Q_FUNC_INFO << "Failed to parse reply";
    }

    //TODO proper checking
    int code = json.value("ocs").toMap().value("meta").toMap().value("statuscode").toInt();

    return code;
}


OcsShareJob::OcsShareJob(const QByteArray &verb, const QUrl &url, const QUrl &postData, AccountPtr account, QObject* parent)
: AbstractNetworkJob(account, "", parent),
  _verb(verb),
  _url(url),
  _postData(postData)

{
    setIgnoreCredentialFailure(true);
}

void OcsShareJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    QBuffer *buffer = new QBuffer;

    QStringList tmp;
    Q_FOREACH(auto tmp2, _postData.queryItems()) {
        tmp.append(tmp2.first + "=" + tmp2.second);
    }
    buffer->setData(tmp.join("&").toAscii());

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
