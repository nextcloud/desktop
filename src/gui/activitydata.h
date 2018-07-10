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

#ifndef ACTIVITYDATA_H
#define ACTIVITYDATA_H

#include <QtCore>
#include <QIcon>

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

class Activity
{
public:
    typedef QPair<qlonglong, QString> Identifier;

    enum Type {
        ActivityType,
        NotificationType,
        ErrorType
    };

    Type _type;
    qlonglong _id;
    QString _object_type;
    QString _subject;
    QString _message;
    QString _file;
    QUrl _link;
    QDateTime _dateTime;
    QString _accName;
    int _status;

    QVector<ActivityLink> _links;
    /**
     * @brief Sort operator to sort the list youngest first.
     * @param val
     * @return
     */


    Identifier ident() const;
};

bool operator==(const Activity &rhs, const Activity &lhs);
bool operator<(const Activity &rhs, const Activity &lhs);

/* ==================================================================== */
/**
 * @brief The ActivityList
 * @ingroup gui
 *
 * A QList based list of Activities
 */

typedef QList<Activity> ActivityList;
}

Q_DECLARE_METATYPE(OCC::Activity::Type)
Q_DECLARE_METATYPE(OCC::ActivityLink)

#endif // ACTIVITYDATA_H
