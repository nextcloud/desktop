#include "sharedialog.h"
#include "ui_sharedialog.h"
#include "networkjobs.h"
#include "account.h"
#include "json.h"
#include "folderman.h"
#include <QBuffer>
#include <QMovie>

namespace {
    int SHARETYPE_PUBLIC = 3;
}

namespace OCC {

ShareDialog::ShareDialog(QWidget *parent) :
    QDialog(parent),
    _ui(new Ui::ShareDialog)
{
    setAttribute(Qt::WA_DeleteOnClose);
    _ui->setupUi(this);
    _ui->pushButton_copy->setIcon(QIcon::fromTheme("edit-copy"));
    layout()->setSizeConstraint(QLayout::SetFixedSize);
    QMovie *movie = new QMovie("/home/azelphur/ownCloud-share-tools/loading-icon.gif");
    movie->start();
    _ui->labelShareSpinner->setMovie(movie);
    _ui->labelShareSpinner->hide();

    _ui->labelPasswordSpinner->setMovie(movie);
    _ui->labelPasswordSpinner->hide();

    _ui->labelCalendarSpinner->setMovie(movie);
    _ui->labelCalendarSpinner->hide();
    connect(_ui->checkBox_shareLink, SIGNAL(clicked()), this, SLOT(slotCheckBoxShareLinkClicked()));
    connect(_ui->checkBox_password, SIGNAL(clicked()), this, SLOT(slotCheckBoxPasswordClicked()));
    connect(_ui->lineEdit_password, SIGNAL(returnPressed()), this, SLOT(slotPasswordReturnPressed()));
    connect(_ui->checkBox_expire, SIGNAL(clicked()), this, SLOT(slotCheckBoxExpireClicked()));
    connect(_ui->calendar, SIGNAL(clicked(QDate)), SLOT(slotCalendarClicked(QDate)));

    _ui->labelShareSpinner->hide();
    _ui->lineEdit_shareLink->hide();
    _ui->pushButton_copy->hide();
    _ui->lineEdit_password->hide();
    _ui->checkBox_password->hide();
    _ui->checkBox_expire->hide();
    _ui->calendar->hide();
    _ui->lineEdit_shareGroup->setPlaceholderText("Share with group...");
    _ui->lineEdit_shareUser->setPlaceholderText("Share with user...");
    _ui->lineEdit_password->setPlaceholderText("Choose a password for the public link");
}

void ShareDialog::setExpireDate(QString date)
{
    _ui->labelCalendarSpinner->show();
    QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/").append(_public_share.value("id").toString()));
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

void ShareDialog::slotExpireSet(QString reply)
{
    _ui->labelCalendarSpinner->hide();
}

void ShareDialog::slotCalendarClicked(QDate date)
{
    ShareDialog::setExpireDate(date.toString("dd-MM-yyyy"));
}

QString ShareDialog::getPath()
{
    return _path;
}

void ShareDialog::setPath(const QString &path)
{
    _path = path;
    ShareDialog::getShares();
}

ShareDialog::~ShareDialog()
{
    delete _ui;
}

void ShareDialog::slotPasswordReturnPressed()
{
    ShareDialog::setPassword(_ui->lineEdit_password->text());
    _ui->lineEdit_password->setPlaceholderText("Password Protected");
    _ui->lineEdit_password->setText("");
}

void ShareDialog::setPassword(QString password)
{
    _ui->labelPasswordSpinner->show();
    QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/").append(_public_share.value("id").toString()));
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

void ShareDialog::slotPasswordSet(QString reply)
{
    _ui->labelPasswordSpinner->hide();
}

void ShareDialog::getShares()
{
    this->setWindowTitle(QString("Sharing ").append(_path));
    QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QLatin1String("ocs/v1.php/apps/files_sharing/api/v1/shares"));
    QList<QPair<QString, QString> > params;
    params.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
    params.append(qMakePair(QString::fromLatin1("path"), _path));
    url.setQueryItems(params);
    OcsShareJob *job = new OcsShareJob("GET", url, QUrl(), AccountManager::instance()->account(), this);
    connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotSharesFetched(QString)));
    job->start();
}

void ShareDialog::slotSharesFetched(QString reply)
{
    bool success = false;
    QVariantMap json = QtJson::parse(reply, success).toMap();
    ShareDialog::_shares = json.value("ocs").toMap().values("data")[0].toList();
    for(int i = 0; i < ShareDialog::_shares.count(); i++)
    {
        QVariantMap data = ShareDialog::_shares[i].toMap();

        if (data.value("share_type").toInt() == SHARETYPE_PUBLIC)
        {
            ShareDialog::_public_share = data;

            _ui->lineEdit_shareLink->show();
            _ui->pushButton_copy->show();
            _ui->checkBox_password->show();
            _ui->checkBox_expire->show();
            _ui->pushButton_copy->show();
            _ui->checkBox_shareLink->setChecked(true);

            if (data.value("share_with").isValid())
            {
                _ui->checkBox_password->setChecked(true);
                _ui->lineEdit_password->setText("********");
                _ui->lineEdit_password->show();
            }

            if (data.value("expire_date").isValid())
            {
                _ui->calendar->setSelectedDate(QDate::fromString(data.value("expire_date").toString(), "dd-MM-yyyy"));
                _ui->calendar->show();
                _ui->checkBox_expire->setChecked(true);
            }

            char url[512];
            sprintf(url, "https://home.azelphur.com/owncloud/public.php?service=files&t=%s", data.value("token").toString().toStdString().c_str());
            _ui->lineEdit_shareLink->setText(url);
        }
    }
}

void ShareDialog::slotDeleteShareFetched(QString reply)
{
    _ui->labelShareSpinner->hide();
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
        _ui->labelShareSpinner->show();
        QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QLatin1String("ocs/v1.php/apps/files_sharing/api/v1/shares"));
        QUrl postData;
        QList<QPair<QString, QString> > getParams;
        QList<QPair<QString, QString> > postParams;
        getParams.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
        postParams.append(qMakePair(QString::fromLatin1("path"), _path));
        postParams.append(qMakePair(QString::fromLatin1("shareType"), QString::number(SHARETYPE_PUBLIC)));
        url.setQueryItems(getParams);
        postData.setQueryItems(postParams);
        OcsShareJob *job = new OcsShareJob("POST", url, postData, AccountManager::instance()->account(), this);
        connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotCreateShareFetched(QString)));
        job->start();
    }
    else
    {
        _ui->labelShareSpinner->show();
        QUrl url = Account::concatUrlPath(AccountManager::instance()->account()->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/").append(_public_share.value("id").toString()));
        QList<QPair<QString, QString> > getParams;
        getParams.append(qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
        url.setQueryItems(getParams);
        OcsShareJob *job = new OcsShareJob("DELETE", url, QUrl(), AccountManager::instance()->account(), this);
        connect(job, SIGNAL(jobFinished(QString)), this, SLOT(slotDeleteShareFetched(QString)));
        job->start();
    }
}

void ShareDialog::slotCreateShareFetched(QString reply)
{
    qDebug() << reply;
    _ui->labelShareSpinner->hide();
    bool success;
    QVariantMap json = QtJson::parse(reply, success).toMap();
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
    }
    else
    {
        ShareDialog::setPassword(QString(""));
        _ui->labelPasswordSpinner->show();
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
        ShareDialog::setExpireDate(QString(""));
        _ui->calendar->hide();
    }
}

OcsShareJob::OcsShareJob(const QByteArray &verb, const QUrl &url, const QUrl &postData, Account* account, QObject* parent)
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
    qDebug() << ">>>>>>>>" << _url;
    QBuffer *buffer = new QBuffer;
    //buffer->setData(_postData.query().toAscii());

    QString tmp;
    auto tmp2 = _postData.queryItems();
    for (int i = 0; i < tmp2.size(); i++) {
        if (i != 0) {
            tmp.append("&");
        }
        tmp.append(tmp2[i].first + "=" + tmp2[i].second);
    }
    buffer->setData(tmp.toAscii());

    setReply(davRequest(_verb, _url, req, buffer));
    //setReply(davRequest("GET", url, req));
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
