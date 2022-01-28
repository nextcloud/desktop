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

#ifndef NOTIFICATIONCONFIRMJOB_H
#define NOTIFICATIONCONFIRMJOB_H

#include "abstractnetworkjob.h"
#include "accountfwd.h"
#include "networkjobs/jsonjob.h"

#include <QVector>
#include <QList>
#include <QPair>
#include <QUrl>

namespace OCC {

class NotificationWidget;

/**
 * @brief The NotificationConfirmJob class
 * @ingroup gui
 *
 * Class to call an action-link of a notification coming from the server.
 * All the communication logic is handled in this class.
 *
 */
class NotificationConfirmJob : public JsonApiJob
{
    Q_OBJECT

public:
    using JsonApiJob::JsonApiJob;

    /**
     * @brief Start the OCS request
     */
    void start() override;

    /**
     * @brief setWidget stores the associated widget to be able to use
     *        it when the job has finished
     * @param widget pointer to the notification widget to store
     */
    void setWidget(NotificationWidget *widget);

    /**
     * @brief widget - get the associated notification widget as stored
     *        with setWidget method.
     * @return widget pointer to the notification widget
     */
    NotificationWidget *widget();

private:
    NotificationWidget *_widget;
};
}

#endif // NotificationConfirmJob_H
