/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "notificationwidget.h"
#include "QProgressIndicator.h"
#include "common/utility.h"
#include "common/asserts.h"
#include "guiutility.h"

#include "ui_notificationwidget.h"

#include "resources/resources.h"

#include <QPushButton>

namespace OCC {

Q_LOGGING_CATEGORY(lcNotifications, "gui.notifications", QtInfoMsg)

NotificationWidget::NotificationWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui_NotificationWidget)
{
    _ui->setupUi(this);
    _progressIndi = new QProgressIndicator(this);
    _ui->horizontalLayout->addWidget(_progressIndi);
}

NotificationWidget::~NotificationWidget()
{
    delete _ui;
}

void NotificationWidget::setActivity(const Activity &activity)
{
    _myActivity = activity;

    _accountName = activity.accName();
    OC_ASSERT(!_accountName.isEmpty());

    // _ui->_headerLabel->setText( );
    _ui->_subjectLabel->setVisible(!activity.subject().isEmpty());
    _ui->_messageLabel->setVisible(!activity.message().isEmpty());

    if (activity.link().isEmpty()) {
        _ui->_subjectLabel->setText(activity.subject());
    } else {
        _ui->_subjectLabel->setText(QStringLiteral("<a href=\"%1\">%2</a>")
                                        .arg(activity.link().toString(QUrl::FullyEncoded),
                                            activity.subject().toHtmlEscaped()));
        _ui->_subjectLabel->setTextFormat(Qt::RichText);
        _ui->_subjectLabel->setOpenExternalLinks(true);
    }

    _ui->_messageLabel->setText(activity.message());
    QString tText = tr("Created at %1").arg(Utility::timeAgoInWords(activity.dateTime()));
    _ui->_timeLabel->setText(tText);

    // always remove the buttons
    qDeleteAll(_buttons);
    _buttons.clear();

    // display buttons for the links
    if (activity.links().isEmpty()) {
        // in case there is no action defined, do a close button.
        QPushButton *b = _ui->_buttonBox->addButton(QDialogButtonBox::Close);
        b->setDefault(true);
        connect(b, &QAbstractButton::clicked, this, [this]{
            QString doneText = tr("Closing in a few seconds...");
            _ui->_timeLabel->setText(doneText);
            emit requestCleanupAndBlacklist(_myActivity);
            return;
        });
    } else {
        for (const auto &link : activity.links()) {
            QPushButton *b = _ui->_buttonBox->addButton(link._label, QDialogButtonBox::AcceptRole);
            b->setDefault(link._isPrimary);
            connect(b, &QAbstractButton::clicked, this, [this, b, link] {
                slotButtonClicked(b, link);
            });
            _buttons.append(b);
        }
    }
}

Activity NotificationWidget::activity() const
{
    return _myActivity;
}

void NotificationWidget::slotButtonClicked(QPushButton *buttonWidget, const ActivityLink &triggeredLink)
{
    buttonWidget->setEnabled(false);
    _actionLabel = triggeredLink._label;
    if (!triggeredLink._link.isEmpty()) {
        qCInfo(lcNotifications) << "Notification Link: " << triggeredLink._verb << triggeredLink._link;
        _progressIndi->startAnimation();
        emit sendNotificationRequest(_accountName, triggeredLink._link, triggeredLink._verb);
    }
}

void NotificationWidget::slotNotificationRequestFinished(bool success)
{
    QString doneText;
    QLocale locale;

    QString timeStr = locale.toString(QTime::currentTime());

    if (success) {
        qCWarning(lcNotifications) << "Notification Request to Server failed, leave button visible.";
        for (auto *button : qAsConst(_buttons)) {
            button->setEnabled(true);
        }
        //: The second parameter is a time, such as 'failed at 09:58pm'
        doneText = tr("%1 request failed at %2").arg(_actionLabel, timeStr);
    } else {
        // the call to the ocs API succeeded.
        _ui->_buttonBox->hide();

        //: The second parameter is a time, such as 'selected at 09:58pm'
        doneText = tr("'%1' selected at %2").arg(_actionLabel, timeStr);
    }
    _ui->_timeLabel->setText(doneText);

    _progressIndi->stopAnimation();
}

void NotificationWidget::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        _ui->_notifIcon->setPixmap(Resources::getCoreIcon(QStringLiteral("bell")).pixmap(_ui->_notifIcon->size()));
        break;
    default:
        break;
    }
    QWidget::changeEvent(e);
}
}
