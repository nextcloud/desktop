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
#include "share.h"

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
    _manager(NULL),
    _share(NULL),
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

    /*
     * Create the share manager and connect it properly
     */
    _manager = new ShareManager(_account, this);

    connect(_manager, SIGNAL(sharesFetched(QList<QSharedPointer<Share>>)), this, SLOT(slotSharesFetched(QList<QSharedPointer<Share>>)));
    connect(_manager, SIGNAL(linkShareCreated(const QSharedPointer<LinkShare>)), this, SLOT(slotCreateShareFetched(const QSharedPointer<LinkShare>)));
    connect(_manager, SIGNAL(linkShareRequiresPassword()), this, SLOT(slotCreateShareRequiresPassword()));
}

void ShareDialog::done( int r ) {
    ConfigFile cfg;
    cfg.saveGeometry(this);
    QDialog::done(r);
}

void ShareDialog::setExpireDate(const QDate &date)
{
    _pi_date->startAnimation();
    _share->setExpireDate(date);
}

void ShareDialog::slotExpireSet()
{
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
    _pi_link->startAnimation();
    _pi_password->startAnimation();

    _ui->checkBox_password->setEnabled(false);
    _ui->lineEdit_password->setEnabled(false);

    if( !_share.isNull() ) {
        _share->setPassword(password);
    } else {
        _ui->checkBox_shareLink->setEnabled(false);
        _manager->createLinkShare(_sharePath, password);
    }
}

void ShareDialog::slotPasswordSet()
{
    /*
     * When setting/deleting a password from a share the old share is
     * deleted and a new one is created. So we need to refetch the shares
     * at this point.
     */
    getShares();

    _pi_password->stopAnimation();
}

void ShareDialog::getShares()
{
    _manager->fetchShares(_sharePath);

    if (QFileInfo(_localPath).isFile()) {
        ThumbnailJob *job2 = new ThumbnailJob(_sharePath, _account, this);
        connect(job2, SIGNAL(jobFinished(int, QByteArray)), SLOT(slotThumbnailFetched(int, QByteArray)));
        job2->start();
    }
}

void ShareDialog::slotSharesFetched(const QList<QSharedPointer<Share>> &shares)
{
    const QString versionString = _account->serverVersion();
    qDebug() << Q_FUNC_INFO << versionString << "Fetched" << shares.count() << "shares";

    //Show link checkbox now
    _ui->checkBox_shareLink->setEnabled(true);
    _pi_link->stopAnimation();

    Q_FOREACH(auto share, shares) {

        if (share->getShareType() == static_cast<int>(OcsShareJob::ShareType::Link)) {
            _share = qSharedPointerDynamicCast<LinkShare>(share);
            _ui->pushButton_copy->show();

            _ui->widget_shareLink->show();
            _ui->checkBox_shareLink->setChecked(true);

            _ui->checkBox_password->setEnabled(true);
            if (_share->isPasswordSet()) {
                _ui->lineEdit_password->setEnabled(true);
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

            _ui->checkBox_expire->setEnabled(true);
            if (_share->getExpireDate().isValid()) {
                _ui->calendar->setDate(_share->getExpireDate());
                _ui->calendar->setMinimumDate(QDate::currentDate().addDays(1));
                _ui->calendar->setEnabled(true);
                _ui->checkBox_expire->setChecked(true);
            } else {
                _ui->calendar->setEnabled(false);
                _ui->checkBox_expire->setChecked(false);
            }

            /*
             * Only directories can have public upload set
             * For public links the server sets CREATE and UPDATE permissions.
             */
            if (!_isFile) {
                _ui->checkBox_editing->setEnabled(true);
                if (_share->getPublicUpload()) {
                    _ui->checkBox_editing->setChecked(true);
                } else {
                    _ui->checkBox_editing->setChecked(false);
                }
            }

            setShareLink(_share->getLink().toString());
            _ui->pushButton_copy->setEnabled(true);

            // Connect all shares signals to gui slots
            connect(_share.data(), SIGNAL(expireDateSet()), this, SLOT(slotExpireSet()));
            connect(_share.data(), SIGNAL(publicUploadSet()), this, SLOT(slotPublicUploadSet()));
            connect(_share.data(), SIGNAL(passwordSet()), this, SLOT(slotPasswordSet()));
            connect(_share.data(), SIGNAL(shareDeleted()), this, SLOT(slotDeleteShareFetched()));

            break;
        }
    }
    if( !_share.isNull() ) {
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

void ShareDialog::slotDeleteShareFetched()
{
    _share.clear();
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
            _ui->checkBox_expire->setEnabled(false);
            _ui->checkBox_editing->setEnabled(false);
            _ui->lineEdit_password->setFocus();
            _ui->pushButton_copy->hide();
            _ui->widget_shareLink->show();

            slotCheckBoxPasswordClicked();
            return;
        }

        _ui->checkBox_shareLink->setEnabled(false);
        _manager->createLinkShare(_sharePath);
    } else {

        if (!_share.isNull()) {
            // We have a share so delete it
            _pi_link->startAnimation();
            _share->deleteShare();
        } else {
            // No share object so we are deleting while a password is required
            _ui->widget_shareLink->hide();
        }

        
    }
}

void ShareDialog::slotCreateShareFetched(const QSharedPointer<LinkShare> share)
{
    _pi_link->stopAnimation();
    _pi_password->stopAnimation();

    _share = share;
    getShares();
}

void ShareDialog::slotCreateShareRequiresPassword()
{
    // there needs to be a password
    _ui->checkBox_password->setChecked(true);
    _ui->checkBox_password->setEnabled(false);
    _ui->checkBox_password->setText(tr("Public sh&aring requires a password"));
    _ui->lineEdit_password->setFocus();
    _ui->pushButton_copy->hide();
    _ui->widget_shareLink->show();
    _ui->checkBox_expire->setEnabled(false);
    _ui->checkBox_editing->setEnabled(false);

    slotCheckBoxPasswordClicked();
}

void ShareDialog::slotCheckBoxPasswordClicked()
{
    if (_ui->checkBox_password->checkState() == Qt::Checked) {
        _ui->lineEdit_password->show();
        _ui->pushButton_setPassword->show();
        _ui->lineEdit_password->setPlaceholderText(tr("Please Set Password"));
        _ui->lineEdit_password->setFocus();
    } else {
        setPassword(QString());
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
        setExpireDate(QDate());
        _ui->calendar->setEnabled(false);
    }
}

#ifdef Q_OS_MAC
extern void copyToPasteboard(const QString &string);
#endif

void ShareDialog::slotPushButtonCopyLinkPressed()
{
#ifdef Q_OS_MAC
    copyToPasteboard(_shareUrl);
#else
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(_shareUrl);
#endif
}

void ShareDialog::slotCheckBoxEditingClicked()
{
    ShareDialog::setPublicUpload(_ui->checkBox_editing->checkState() == Qt::Checked);
}

void ShareDialog::setPublicUpload(bool publicUpload)
{
    _ui->checkBox_editing->setEnabled(false);
    _pi_editing->startAnimation();

    _share->setPublicUpload(publicUpload);
}

void ShareDialog::slotPublicUploadSet()
{
    _pi_editing->stopAnimation();
    _ui->checkBox_editing->setEnabled(true);
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
