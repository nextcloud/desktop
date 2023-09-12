/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "gui/filedetails/datefieldbackend.h"

#include <QTest>
#include <QSignalSpy>

using namespace OCC;

class TestDateFieldBackend : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultBehaviour()
    {
        constexpr auto dateStringFormat = "dd/MM/yyyy";

        Quick::DateFieldBackend backend;
        backend._dateFormat = dateStringFormat;

        const auto currentDate = QDate::currentDate();
        const auto currentDateMSecs = currentDate.startOfDay(Qt::UTC).toMSecsSinceEpoch();
        const auto currentDateString = currentDate.toString(dateStringFormat);

        QCOMPARE(backend.date(), currentDate);
        QCOMPARE(backend.dateMsecs(), currentDateMSecs);
        QCOMPARE(backend.dateString(), currentDateString);
    }

    void testDateBoundaries()
    {
        Quick::DateFieldBackend backend;
        const auto minDate = QDate::currentDate().addDays(-5);
        const auto maxDate = QDate::currentDate().addDays(5);
        const auto minDateMs = minDate.startOfDay(Qt::UTC).toMSecsSinceEpoch();
        const auto maxDateMs = maxDate.startOfDay(Qt::UTC).toMSecsSinceEpoch();
        const auto invalidMinDate = minDate.addDays(-1);
        const auto invalidMaxDate = maxDate.addDays(1);

        // Set by QDate
        backend.setMinimumDate(minDate);
        backend.setMaximumDate(maxDate);

        QCOMPARE(backend.minimumDate(), minDate);
        QCOMPARE(backend.maximumDate(), maxDate);
        QCOMPARE(backend.minimumDateMsecs(), minDateMs);
        QCOMPARE(backend.maximumDateMsecs(), maxDateMs);

        // Reset and try when setting by MSecs
        backend.setMinimumDate({});
        backend.setMaximumDate({});
        backend.setMinimumDateMsecs(minDateMs);
        backend.setMaximumDateMsecs(maxDateMs);

        QCOMPARE(backend.minimumDate(), minDate);
        QCOMPARE(backend.maximumDate(), maxDate);
        QCOMPARE(backend.minimumDateMsecs(), minDateMs);
        QCOMPARE(backend.maximumDateMsecs(), maxDateMs);

        // Since we default to the current date, the date should be valid
        QVERIFY(backend.validDate());

        // Now try with invalid dates
        backend.setDate(invalidMinDate);
        QVERIFY(!backend.validDate());

        backend.setDate(invalidMaxDate);
        QVERIFY(!backend.validDate());
    }
};

QTEST_MAIN(TestDateFieldBackend)
#include "testdatefieldbackend.moc"
