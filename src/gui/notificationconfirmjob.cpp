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

#include "notificationconfirmjob.h"
#include "account.h"
#include "networkjobs.h"
#include "networkjobs/jsonjob.h"

#include <QBuffer>

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcNotifications)

void NotificationConfirmJob::setWidget(NotificationWidget *widget)
{
    _widget = widget;
}

NotificationWidget *NotificationConfirmJob::widget()
{
    return _widget;
}

void NotificationConfirmJob::start()
{
    setForceIgnoreCredentialFailure(true);

    JsonApiJob::start();
}

}
