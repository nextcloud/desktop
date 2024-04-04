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

#pragma once

#include "gui/owncloudguilib.h"

#include "account.h"

#include <QtCore>

namespace OCC {
/**
 * @brief The ActivityLink class describes actions of an activity
 *
 * These are part of notifications which are mapped into activities.
 */

class ActivityLink
{
public:
    QString _label;
    QString _link;
    QByteArray _verb;
    bool _isPrimary;
};

/* ==================================================================== */
/**
 * @brief Activity Structure
 * @ingroup gui
 *
 * contains all the information describing a single activity.
 */

class OWNCLOUDGUI_EXPORT Activity
{
public:
    enum Type {
        ActivityType,
        NotificationType
    };
    Activity() = default;
    explicit Activity(Type type, const QString &id, AccountPtr acc, const QString &subject, const QString &message, const QString &file, const QUrl &link,
        const QDateTime &dateTime, const QVector<ActivityLink> &&links = {});

    Type type() const;

    QString id() const;

    QString subject() const;

    QString message() const;

    QString file() const;

    QUrl link() const;

    QDateTime dateTime() const;

    QString accName() const;

    QUuid accountUuid() const;

    const QVector<ActivityLink> &links() const;

    bool operator==(const Activity &lhs) const;

private:
    Type _type;
    QString _id;
    QString _accName; /* display name of the account */
    QUuid _uuid; /* uuid of the account */
    QString _subject;
    QString _message;
    QString _file;
    QUrl _link;
    QDateTime _dateTime;

    QVector<ActivityLink> _links; /* These links are transformed into buttons that
                                   * call links as reactions on the activity */
};


/* ==================================================================== */
/**
 * @brief The ActivityList
 * @ingroup gui
 *
 * A QList based list of Activities
 */

typedef QList<Activity> ActivityList;
}
