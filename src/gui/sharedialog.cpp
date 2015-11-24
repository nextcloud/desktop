/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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
#include "sharelinkwidget.h"
#include "shareusergroupwidget.h"
#include "ui_sharedialog.h"

#include "account.h"
#include "configfile.h"
#include "theme.h"
#include "thumbnailjob.h"

#include <QFileInfo>
#include <QFileIconProvider>
#include <QDebug>
#include <QPushButton>
#include <QFrame>

namespace OCC {

ShareDialog::ShareDialog(AccountPtr account, const QString &sharePath, const QString &localPath, bool resharingAllowed, QWidget *parent) :
    QDialog(parent),
    _ui(new Ui::ShareDialog),
    _account(account),
    _sharePath(sharePath),
    _localPath(localPath),
    _resharingAllowed(resharingAllowed),
    _linkWidget(NULL),
    _userGroupWidget(NULL)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("SharingDialog"); // required as group for saveGeometry call

    _ui->setupUi(this);

    QPushButton *closeButton = _ui->buttonBox->button(QDialogButtonBox::Close);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));

    // Because people press enter in the dialog and we don't want to close for that
    closeButton->setDefault(false);
    closeButton->setAutoDefault(false);

    // Set icon
    QFileInfo f_info(_localPath);
    QFileIconProvider icon_provider;
    QIcon icon = icon_provider.icon(f_info);
    _ui->label_icon->setPixmap(icon.pixmap(40,40));

    // Set filename
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

    // Laying this out is complex because sharePath
    // may be in use or not.
    _ui->gridLayout->removeWidget(_ui->label_sharePath);
    _ui->gridLayout->removeWidget(_ui->label_name);
    if( ocDir.isEmpty() ) {
        _ui->gridLayout->addWidget(_ui->label_name, 0, 1, 2, 1);
        _ui->label_sharePath->setText(QString());
    } else {
        _ui->gridLayout->addWidget(_ui->label_name, 0, 1, 1, 1);
        _ui->gridLayout->addWidget(_ui->label_sharePath, 1, 1, 1, 1);
        _ui->label_sharePath->setText(tr("Folder: %2").arg(ocDir));
    }

    this->setWindowTitle(tr("%1 Sharing").arg(Theme::instance()->appNameGUI()));

    if (!account->capabilities().shareAPI()) {
        _ui->shareWidgetsLayout->addWidget(new QLabel(tr("The server does not allow sharing")));
        return;
    }

    bool autoShare = true;

    // We only do user/group sharing from 8.2.0
    if (account->serverVersionInt() >= ((8 << 16) + (2 << 8))) {
        _userGroupWidget = new ShareUserGroupWidget(account, sharePath, localPath, resharingAllowed, this);
        _ui->shareWidgetsLayout->addWidget(_userGroupWidget);


        QFrame *hline = new QFrame(this);
        hline->setFrameShape(QFrame::HLine);
        QPalette p = palette();
        // Make the line softer:
        p.setColor(QPalette::Foreground, QColor::fromRgba((p.color(QPalette::Foreground).rgba() & 0x00ffffff) | 0x50000000));
        hline->setPalette(p);
        _ui->shareWidgetsLayout->addWidget(hline);


        autoShare = false;
    }

    _linkWidget = new ShareLinkWidget(account, sharePath, localPath, resharingAllowed, autoShare, this);
    _linkWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    _ui->shareWidgetsLayout->addWidget(_linkWidget);
}

ShareDialog::~ShareDialog()
{
    delete _ui;
}

void ShareDialog::done( int r ) {
    ConfigFile cfg;
    cfg.saveGeometry(this);
    QDialog::done(r);
}

void ShareDialog::getShares()
{
    if (QFileInfo(_localPath).isFile()) {
        ThumbnailJob *job = new ThumbnailJob(_sharePath, _account, this);
        connect(job, SIGNAL(jobFinished(int, QByteArray)), SLOT(slotThumbnailFetched(int, QByteArray)));
        job->start();
    }

    if (_linkWidget) {
        _linkWidget->getShares();
    }
    if (_userGroupWidget != NULL) {
        _userGroupWidget->getShares();
    }
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

