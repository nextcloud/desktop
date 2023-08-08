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
#include <QRegularExpression>

namespace OCC
{
namespace Quick
{

DateFieldBackend::DateFieldBackend(QObject *const parent)
    : QObject(parent)
{
    // Ensure the date format is for a full year. QLocale::ShortFormat often
    // provides a short year format that is only two years, which is an absolute
    // pain to work with -- ensure instead we have the full, unambiguous year
    _dateFormat = QLocale::system().dateFormat(QLocale::ShortFormat);
    // Check for specifically two y's, no more and no fewer, within format date
    const QRegularExpression re("(?<!y)y{2}(?!y)");

    if (auto match = re.match(_dateFormat); match.hasMatch()) {
        _dateFormat.replace(match.capturedStart(), match.capturedLength(), "yyyy");
    }
}

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
    Q_EMIT dateTimeMsecsChanged();
    Q_EMIT dateTimeStringChanged();
    Q_EMIT validDateTimeChanged();
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
    return _dateTime.toString(_dateFormat);
}

void DateFieldBackend::setDateTimeString(const QString &dateTimeString)
{
    const auto locale = QLocale::system();
    const auto dt = locale.toDateTime(dateTimeString, _dateFormat);
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
    Q_EMIT minimumDateTimeMsecsChanged();
    Q_EMIT validDateTimeChanged();
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
    Q_EMIT maximumDateTimeMsecsChanged();
    Q_EMIT validDateTimeChanged();
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

bool DateFieldBackend::validDateTime() const
{
    auto valid = _dateTime.isValid();

    if (_minimumDateTime.isValid()) {
        valid &= _dateTime >= _minimumDateTime;
    }

    if (_maximumDateTime.isValid()) {
        valid &= _dateTime <= _maximumDateTime;
    }

    return valid;
}
}
}