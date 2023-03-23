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

#include <QtCore>

#include "activitydata.h"


namespace OCC {

Activity::Activity(Activity::Type type, const QString &id, AccountPtr acc, const QString &subject, const QString &message, const QString &file,
    const QUrl &link, const QDateTime &dateTime, const QVector<ActivityLink> &&links)
    : _type(type)
    , _id(id)
    , _accName(acc->displayName())
    , _uuid(acc->uuid())
    , _subject(subject)
    , _message(message)
    , _file(file)
    , _link(link)
    , _dateTime(dateTime)
    , _links(links)
{
}

Activity::Type Activity::type() const
{
    return _type;
}

QString Activity::id() const
{
    return _id;
}

QString Activity::subject() const
{
    return _subject;
}

QString Activity::message() const
{
    return _message;
}

QString Activity::file() const
{
    return _file;
}

QUrl Activity::link() const
{
    return _link;
}

QDateTime Activity::dateTime() const
{
    return _dateTime;
}

QString Activity::accName() const
{
    return _accName;
}

QUuid Activity::accountUuid() const
{
    return _uuid;
}

const QVector<ActivityLink> &Activity::links() const
{
    return _links;
}

bool Activity::operator==(const Activity &lhs) const
{
    return (_type == lhs._type && _id == lhs._id && _uuid == lhs._uuid);
}
}
