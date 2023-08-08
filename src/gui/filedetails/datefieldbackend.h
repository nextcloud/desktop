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

#include <QDateTime>
#include <QObject>

namespace OCC
{
namespace Quick
{

class DateFieldBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QDateTime dateTime READ dateTime WRITE setDateTime NOTIFY dateTimeChanged)
    Q_PROPERTY(qint64 dateTimeMsecs READ dateTimeMsecs WRITE setDateTimeMsecs NOTIFY dateTimeChanged)
    Q_PROPERTY(QString dateTimeString READ dateTimeString WRITE setDateTimeString NOTIFY dateTimeChanged)

    Q_PROPERTY(QDateTime minimumDateTime READ minimumDateTime WRITE setMinimumDateTime NOTIFY minimumDateTimeChanged)
    Q_PROPERTY(qint64 minimumDateTimeMsecs READ minimumDateTimeMsecs WRITE setMinimumDateTimeMsecs NOTIFY minimumDateTimeChanged)

    Q_PROPERTY(QDateTime maximumDateTime READ maximumDateTime WRITE setMaximumDateTime NOTIFY maximumDateTimeChanged)
    Q_PROPERTY(qint64 maximumDateTimeMsecs READ maximumDateTimeMsecs WRITE setMaximumDateTimeMsecs NOTIFY maximumDateTimeChanged)

    Q_PROPERTY(bool validDateTime READ validDateTime NOTIFY dateTimeChanged NOTIFY minimumDateTimeChanged NOTIFY maximumDateTimeChanged)

public:
    explicit DateFieldBackend(QObject *const parent = nullptr);

    [[nodiscard]] QDateTime dateTime() const;
    [[nodiscard]] qint64 dateTimeMsecs() const;
    [[nodiscard]] QString dateTimeString() const;

    [[nodiscard]] QDateTime minimumDateTime() const;
    [[nodiscard]] qint64 minimumDateTimeMsecs() const;

    [[nodiscard]] QDateTime maximumDateTime() const;
    [[nodiscard]] qint64 maximumDateTimeMsecs() const;

    [[nodiscard]] bool validDateTime() const;

public slots:
    void setDateTime(const QDateTime &dateTime);
    void setDateTimeMsecs(const qint64 dateTimeMsecs);
    void setDateTimeString(const QString &dateTimeString);

    void setMinimumDateTime(const QDateTime &minimumDateTime);
    void setMinimumDateTimeMsecs(const qint64 minimumDateTimeMsecs);

    void setMaximumDateTime(const QDateTime &maximumDateTime);
    void setMaximumDateTimeMsecs(const qint64 maximumDateTimeMsecs);

signals:
    void dateTimeChanged();
    void minimumDateTimeChanged();
    void maximumDateTimeChanged();

private:
    QDateTime _dateTime = QDateTime::currentDateTimeUtc();
    QDateTime _minimumDateTime;
    QDateTime _maximumDateTime;

    QString _dateFormat;
};

} // namespace Quick
} // namespace OCC