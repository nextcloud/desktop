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
    ASSERT(!_accountName.isEmpty());

    // _ui._headerLabel->setText( );
    _ui._subjectLabel->setVisible(!activity._subject.isEmpty());
    _ui._messageLabel->setVisible(!activity._message.isEmpty());

    _ui._subjectLabel->setText(activity._subject);
    _ui._messageLabel->setText(activity._message);

    _ui._notifIcon->setPixmap(QPixmap(":/client/resources/bell.png"));
    _ui._notifIcon->setMinimumWidth(64);
    _ui._notifIcon->setMinimumHeight(64);
    _ui._notifIcon->show();

    QString tText = tr("Created at %1").arg(Utility::timeAgoInWords(activity._dateTime));
    _ui._timeLabel->setText(tText);

    // always remove the buttons
    foreach (auto button, _ui._buttonBox->buttons()) {
        _ui._buttonBox->removeButton(button);
    }
    _buttons.clear();

    // display buttons for the links
    if (activity._links.isEmpty()) {
        // in case there is no action defined, do a close button.
        QPushButton *b = _ui._buttonBox->addButton(QDialogButtonBox::Close);
        b->setDefault(true);
        connect(b, &QAbstractButton::clicked, this, &NotificationWidget::slotButtonClicked);
        _buttons.append(b);
    } else {
        foreach (auto link, activity._links) {
            QPushButton *b = _ui._buttonBox->addButton(link._label, QDialogButtonBox::AcceptRole);
            b->setDefault(link._isPrimary);
            connect(b, &QAbstractButton::clicked, this, &NotificationWidget::slotButtonClicked);
            _buttons.append(b);
        }
    }
}

Activity NotificationWidget::activity() const
{
    return _myActivity;
}

void NotificationWidget::slotButtonClicked()
{
    QObject *buttonWidget = QObject::sender();
    int index = -1;
    if (buttonWidget) {
        // find the button that was clicked, it has to be in the list
        // of buttons that were added to the button box before.
        for (int i = 0; i < _buttons.count(); i++) {
            if (_buttons.at(i) == buttonWidget) {
                index = i;
            }
            _buttons.at(i)->setEnabled(false);
        }

        // if the button was found, the link must be called
        if (index > -1 && _myActivity._links.count() == 0) {
            // no links, that means it was the close button
            // empty link. Just close and remove the widget.
            QString doneText = tr("Closing in a few seconds...");
            _ui._timeLabel->setText(doneText);
            emit requestCleanupAndBlacklist(_myActivity);
            return;
        }

        if (index > -1 && index < _myActivity._links.count()) {
            ActivityLink triggeredLink = _myActivity._links.at(index);
            _actionLabel = triggeredLink._label;

            if (!triggeredLink._link.isEmpty()) {
                qCInfo(lcNotifications) << "Notification Link: " << triggeredLink._verb << triggeredLink._link;
                _progressIndi->startAnimation();
                emit sendNotificationRequest(_accountName, triggeredLink._link, triggeredLink._verb);
            }
        }
    }
}

void NotificationWidget::slotNotificationRequestFinished(int statusCode)
{
    int i = 0;
    QString doneText;
    QLocale locale;

    QString timeStr = locale.toString(QTime::currentTime());

    // the ocs API returns stat code 100 if it succeeded.
    if (statusCode != OCS_SUCCESS_STATUS_CODE) {
        qCWarning(lcNotifications) << "Notification Request to Server failed, leave button visible.";
        for (i = 0; i < _buttons.count(); i++) {
            _buttons.at(i)->setEnabled(true);
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
