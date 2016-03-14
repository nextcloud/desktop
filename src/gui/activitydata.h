/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef ACTIVITYDATA_H
#define ACTIVITYDATA_H

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
    QVariantHash toVariantHash() const {
        QVariantHash hash;

        hash["label"] = _label;
        hash["link"]  = _link;
        hash["verb"]  = _verb;
        hash["primary"] = _isPrimary;

        return hash;
    }

    QString _label;
    QString _link;
    QString _verb;
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
    enum Type {
        ActivityType,
        NotificationType
    };
    Type      _type;
    qlonglong _id;
    QString   _subject;
    QString   _message;
    QString   _file;
    QUrl      _link;
    QDateTime _dateTime;
    QString   _accName;

    QVector <ActivityLink> _links;
    /**
     * @brief Sort operator to sort the list youngest first.
     * @param val
     * @return
     */
    bool operator<( const Activity& val ) const {
        return _dateTime.toMSecsSinceEpoch() > val._dateTime.toMSecsSinceEpoch();
    }

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

#endif // ACTIVITYDATA_H
