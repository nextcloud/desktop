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

#include <QPushButton>

#include "ocsjob.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcNotifications, "gui.notifications", QtInfoMsg)

NotificationWidget::NotificationWidget(QWidget *parent)
    : QWidget(parent)
{
    _ui.setupUi(this);
    _progressIndi = new QProgressIndicator(this);
    _ui.horizontalLayout->addWidget(_progressIndi);
}

void NotificationWidget::setActivity(const Activity &activity)
{
    _myActivity = activity;

    _accountName = activity._accName;
    OC_ASSERT(!_accountName.isEmpty());

    // _ui._headerLabel->setText( );
    _ui._subjectLabel->setVisible(!activity._subject.isEmpty());
    _ui._messageLabel->setVisible(!activity._message.isEmpty());

    if (activity._link.isEmpty()) {
        _ui._subjectLabel->setText(activity._subject);
    } else {
        _ui._subjectLabel->setText( QString("<a href=\"%1\">%2</a>")
                    .arg(activity._link.toString(QUrl::FullyEncoded),
                         activity._subject.toHtmlEscaped() ));
        _ui._subjectLabel->setTextFormat(Qt::RichText);
        _ui._subjectLabel->setOpenExternalLinks(true);
    }

    _ui._messageLabel->setText(activity._message);
    QString tText = tr("Created at %1").arg(Utility::timeAgoInWords(activity._dateTime));
    _ui._timeLabel->setText(tText);

    // always remove the buttons
    qDeleteAll(_buttons);
    _buttons.clear();

    // display buttons for the links
    if (activity._links.isEmpty()) {
        // in case there is no action defined, do a close button.
        QPushButton *b = _ui._buttonBox->addButton(QDialogButtonBox::Close);
        b->setDefault(true);
        connect(b, &QAbstractButton::clicked, this, [this]{
            QString doneText = tr("Closing in a few seconds...");
            _ui._timeLabel->setText(doneText);
            emit requestCleanupAndBlacklist(_myActivity);
            return;
        });
    } else {
        for (const auto &link : activity._links) {
            QPushButton *b = _ui._buttonBox->addButton(link._label, QDialogButtonBox::AcceptRole);
            b->setDefault(link._isPrimary);
            connect(b, &QAbstractButton::clicked, this, [this, b, &link]{
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

void NotificationWidget::slotNotificationRequestFinished(int statusCode)
{
    QString doneText;
    QLocale locale;

    QString timeStr = locale.toString(QTime::currentTime());

    // the ocs API returns stat code 100 or 200 inside the xml if it succeeded.
    if (statusCode != OCS_SUCCESS_STATUS_CODE && statusCode != OCS_SUCCESS_STATUS_CODE_V2) {
        qCWarning(lcNotifications) << "Notification Request to Server failed, leave button visible.";
        for (auto button : _buttons) {
            button->setEnabled(true);
        }
        //: The second parameter is a time, such as 'failed at 09:58pm'
        doneText = tr("%1 request failed at %2").arg(_actionLabel, timeStr);
    } else {
        // the call to the ocs API succeeded.
        _ui._buttonBox->hide();

        //: The second parameter is a time, such as 'selected at 09:58pm'
        doneText = tr("'%1' selected at %2").arg(_actionLabel, timeStr);
    }
    _ui._timeLabel->setText(doneText);

    _progressIndi->stopAnimation();
}
}
