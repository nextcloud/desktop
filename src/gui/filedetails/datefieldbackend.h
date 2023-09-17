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

#pragma once

#include <QDate>
#include <QObject>

class TestDateFieldBackend;

namespace OCC
{
namespace Quick
{

class DateFieldBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QDate date READ date WRITE setDate NOTIFY dateChanged)
    Q_PROPERTY(qint64 dateMsecs READ dateMsecs WRITE setDateMsecs NOTIFY dateMsecsChanged)
    Q_PROPERTY(QString dateString READ dateString WRITE setDateString NOTIFY dateStringChanged)

    Q_PROPERTY(QDate minimumDate READ minimumDate WRITE setMinimumDate NOTIFY minimumDateChanged)
    Q_PROPERTY(qint64 minimumDateMsecs READ minimumDateMsecs WRITE setMinimumDateMsecs NOTIFY minimumDateMsecsChanged)

    Q_PROPERTY(QDate maximumDate READ maximumDate WRITE setMaximumDate NOTIFY maximumDateChanged)
    Q_PROPERTY(qint64 maximumDateMsecs READ maximumDateMsecs WRITE setMaximumDateMsecs NOTIFY maximumDateMsecsChanged)

    Q_PROPERTY(bool validDate READ validDate NOTIFY validDateChanged)

public:
    explicit DateFieldBackend(QObject *const parent = nullptr);

    [[nodiscard]] QDate date() const;
    [[nodiscard]] qint64 dateMsecs() const;
    [[nodiscard]] QString dateString() const;

    [[nodiscard]] QDate minimumDate() const;
    [[nodiscard]] qint64 minimumDateMsecs() const;

    [[nodiscard]] QDate maximumDate() const;
    [[nodiscard]] qint64 maximumDateMsecs() const;

    [[nodiscard]] bool validDate() const;

public slots:
    void setDate(const QDate &date);
    void setDateMsecs(const qint64 dateMsecs);
    void setDateString(const QString &dateString);

    void setMinimumDate(const QDate &minimumDate);
    void setMinimumDateMsecs(const qint64 minimumDateMsecs);

    void setMaximumDate(const QDate &maximumDate);
    void setMaximumDateMsecs(const qint64 maximumDateMsecs);

signals:
    void dateChanged();
    void dateMsecsChanged();
    void dateStringChanged();

    void minimumDateChanged();
    void minimumDateMsecsChanged();

    void maximumDateChanged();
    void maximumDateMsecsChanged();

    void validDateChanged();

private:
    friend class ::TestDateFieldBackend;

    QDate _date = QDate::currentDate();
    QDate _minimumDate;
    QDate _maximumDate;

    QString _dateFormat;
    QString _leadingZeroMonthDateFormat;
};

} // namespace Quick
} // namespace OCC
