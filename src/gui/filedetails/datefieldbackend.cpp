/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "datefieldbackend.h"

#include <QLocale>

namespace OCC
{
namespace Quick
{

QDateTime DateFieldBackend::dateTime() const
{
    return _dateTime;
}

void DateFieldBackend::setDateTime(const QDateTime &dateTime)
{
    if (_dateTime == dateTime) {
        return;
    }

    _dateTime = dateTime;
    Q_EMIT dateTimeChanged();
}

qint64 DateFieldBackend::dateTimeMsecs() const
{
    return _dateTime.toMSecsSinceEpoch();
}

void DateFieldBackend::setDateTimeMsecs(const qint64 dateTimeMsecs)
{
    if (_dateTime.toMSecsSinceEpoch() == dateTimeMsecs) {
        return;
    }

    const auto dt = QDateTime::fromMSecsSinceEpoch(dateTimeMsecs);
    setDateTime(dt);
}

QString DateFieldBackend::dateTimeString() const
{
    const auto locale = QLocale::system();
    return _dateTime.toString(locale.dateTimeFormat(QLocale::ShortFormat));
}

void DateFieldBackend::setDateTimeString(const QString &dateTimeString)
{
    const auto locale = QLocale::system();
    const auto dt = locale.toDateTime(dateTimeString, locale.dateTimeFormat(QLocale::ShortFormat));
    setDateTime(dt);
}

QDateTime DateFieldBackend::minimumDateTime() const
{
    return _minimumDateTime;
}

void DateFieldBackend::setMinimumDateTime(const QDateTime &minimumDateTime)
{
    if (_minimumDateTime == minimumDateTime) {
        return;
    }

    _minimumDateTime = minimumDateTime;
    Q_EMIT minimumDateTimeChanged();
}

qint64 DateFieldBackend::minimumDateTimeMsecs() const
{
    return _minimumDateTime.toMSecsSinceEpoch();
}

void DateFieldBackend::setMinimumDateTimeMsecs(const qint64 minimumDateTimeMsecs)
{
    if (_minimumDateTime.toMSecsSinceEpoch() == minimumDateTimeMsecs) {
        return;
    }

    const auto dt = QDateTime::fromMSecsSinceEpoch(minimumDateTimeMsecs);
    setMinimumDateTime(dt);
}

QDateTime DateFieldBackend::maximumDateTime() const
{
    return _maximumDateTime;
}

void DateFieldBackend::setMaximumDateTime(const QDateTime &maximumDateTime)
{
    if (_maximumDateTime == maximumDateTime) {
        return;
    }

    _maximumDateTime = maximumDateTime;
    Q_EMIT maximumDateTimeChanged();
}

qint64 DateFieldBackend::maximumDateTimeMsecs() const
{
    return _maximumDateTime.toMSecsSinceEpoch();
}

void DateFieldBackend::setMaximumDateTimeMsecs(const qint64 maximumDateTimeMsecs)
{
    if (_maximumDateTime.toMSecsSinceEpoch() == maximumDateTimeMsecs) {
        return;
    }

    const auto dt = QDateTime::fromMSecsSinceEpoch(maximumDateTimeMsecs);
    setMaximumDateTime(dt);
}
}
}