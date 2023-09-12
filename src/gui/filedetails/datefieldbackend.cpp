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
#include <QTimeZone>

namespace OCC
{
namespace Quick
{

DateFieldBackend::DateFieldBackend(QObject *const parent)
    : QObject(parent)
{
    _dateFormat = QLocale::system().dateFormat(QLocale::ShortFormat);

    // Ensure the date format is for a full year. QLocale::ShortFormat often
    // provides a short year format that is only two years, which is an absolute
    // pain to work with -- ensure instead we have the full, unambiguous year.
    // Check for specifically two y's, no more and no fewer, within format date
    const QRegularExpression yearRe("(?<!y)y{2}(?!y)");

    // To prevent invalid parsings when the user submits a month with a leading
    // zero, also add an alternative date format that checks with a leading zero
    // This regex only matches, e.g. dd/M/yyyy which often is the default for
    // short locale date formats, which removes the leading 0
    const QRegularExpression monthRe("(?<!M)M{1}(?!M)");

    if (const auto match = yearRe.match(_dateFormat); match.hasMatch()) {
        _dateFormat.replace(match.capturedStart(), match.capturedLength(), "yyyy");
    }

    _leadingZeroMonthDateFormat = _dateFormat;

    if (const auto match = monthRe.match(_dateFormat); match.hasMatch()) {
        _leadingZeroMonthDateFormat.replace(match.capturedStart(), match.capturedLength(), "MM");
    }
}

QDate DateFieldBackend::date() const
{
    return _date;
}

void DateFieldBackend::setDate(const QDate &date)
{
    if (_date == date) {
        return;
    }

    _date = date;

    Q_EMIT dateChanged();
    Q_EMIT dateMsecsChanged();
    Q_EMIT dateStringChanged();
    Q_EMIT validDateChanged();
}

qint64 DateFieldBackend::dateMsecs() const
{
    return _date.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch();
}

void DateFieldBackend::setDateMsecs(const qint64 dateMsecs)
{
    if (_date.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch() == dateMsecs) {
        return;
    }

    const auto dt = QDateTime::fromMSecsSinceEpoch(dateMsecs).toUTC();
    setDate(dt.date());
}

QString DateFieldBackend::dateString() const
{
    return _date.toString(_dateFormat);
}

void DateFieldBackend::setDateString(const QString &dateString)
{
    const auto locale = QLocale::system();
    auto date = locale.toDate(dateString, _dateFormat);

    if (!date.isValid()) {
        date = locale.toDate(dateString, _leadingZeroMonthDateFormat);
    }

    setDate(date);
}

QDate DateFieldBackend::minimumDate() const
{
    return _minimumDate;
}

void DateFieldBackend::setMinimumDate(const QDate &minimumDate)
{
    if (_minimumDate == minimumDate) {
        return;
    }

    _minimumDate = minimumDate;
    Q_EMIT minimumDateChanged();
    Q_EMIT minimumDateMsecsChanged();
    Q_EMIT validDateChanged();
}

qint64 DateFieldBackend::minimumDateMsecs() const
{
    return _minimumDate.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch();
}

void DateFieldBackend::setMinimumDateMsecs(const qint64 minimumDateMsecs)
{
    if (_minimumDate.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch() == minimumDateMsecs) {
        return;
    }

    const auto dt = QDateTime::fromMSecsSinceEpoch(minimumDateMsecs);
    setMinimumDate(dt.date());
}

QDate DateFieldBackend::maximumDate() const
{
    return _maximumDate;
}

void DateFieldBackend::setMaximumDate(const QDate &maximumDate)
{
    if (_maximumDate == maximumDate) {
        return;
    }

    _maximumDate = maximumDate;
    Q_EMIT maximumDateChanged();
    Q_EMIT maximumDateMsecsChanged();
    Q_EMIT validDateChanged();
}

qint64 DateFieldBackend::maximumDateMsecs() const
{
    return _maximumDate.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch();
}

void DateFieldBackend::setMaximumDateMsecs(const qint64 maximumDateMsecs)
{
    if (_maximumDate.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch() == maximumDateMsecs) {
        return;
    }

    const auto dt = QDateTime::fromMSecsSinceEpoch(maximumDateMsecs);
    setMaximumDate(dt.date());
}

bool DateFieldBackend::validDate() const
{
    auto valid = _date.isValid();

    if (_minimumDate.isValid() && minimumDateMsecs() > 0) {
        valid &= _date >= _minimumDate;
    }

    if (_maximumDate.isValid() && maximumDateMsecs() > 0) {
        valid &= _date <= _maximumDate;
    }

    return valid;
}
}
}