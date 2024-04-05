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

#ifndef NOTIFICATIONWIDGET_H
#define NOTIFICATIONWIDGET_H

#include <QWidget>

#include "activitydata.h"

class QProgressIndicator;
class QPushButton;

class Ui_NotificationWidget;

namespace OCC {

class NotificationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NotificationWidget(QWidget *parent = nullptr);
    ~NotificationWidget();

    bool readyToClose();
    Activity activity() const;

Q_SIGNALS:
    void sendNotificationRequest(const QString &, const QString &link, const QByteArray &verb);
    void requestCleanupAndBlacklist(const Activity &activity);

public Q_SLOTS:
    void setActivity(const Activity &activity);
    void slotNotificationRequestFinished(bool success);

protected:
    void changeEvent(QEvent *) override;

private:
    void slotButtonClicked(QPushButton *buttonWidget, const ActivityLink &triggeredLink);

    Ui_NotificationWidget *_ui;
    Activity _myActivity;
    QList<QPushButton *> _buttons;
    QString _accountName;
    QProgressIndicator *_progressIndi;
    QString _actionLabel;
};
}

#endif // NOTIFICATIONWIDGET_H
