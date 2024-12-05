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

#include "logger.h"

#include <QTest>
#include <QSignalSpy>
#include <QStandardPaths>

using namespace OCC;

class TestDateFieldBackend : public QObject
{
    Q_OBJECT

private:
    static constexpr auto dateStringFormat = "dd/MM/yyyy";

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testDefaultBehaviour()
    {
        Quick::DateFieldBackend backend;
        backend._dateFormat = dateStringFormat;

        const auto currentDate = QDate::currentDate();
        const auto currentDateMSecs = currentDate.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch();
        const auto currentDateString = currentDate.toString(dateStringFormat);

        QCOMPARE(backend.date(), currentDate);
        QCOMPARE(backend.dateMsecs(), currentDateMSecs);
        QCOMPARE(backend.dateString(), currentDateString);
    }

    void testDateBoundaries()
    {
        Quick::DateFieldBackend backend;

        QSignalSpy minimumDateChangedSpy(&backend, &Quick::DateFieldBackend::minimumDateChanged);
        QSignalSpy maximumDateChangedSpy(&backend, &Quick::DateFieldBackend::maximumDateChanged);
        QSignalSpy minimumDateMsecsChangedSpy(&backend, &Quick::DateFieldBackend::minimumDateMsecsChanged);
        QSignalSpy maximumDateMsecsChangedSpy(&backend, &Quick::DateFieldBackend::maximumDateMsecsChanged);
        QSignalSpy validDateChangedSpy(&backend, &Quick::DateFieldBackend::validDateChanged);

        const auto minDate = QDate::currentDate().addDays(-5);
        const auto maxDate = QDate::currentDate().addDays(5);
        const auto minDateMs = minDate.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch();
        const auto maxDateMs = maxDate.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch();
        const auto invalidMinDate = minDate.addDays(-1);
        const auto invalidMaxDate = maxDate.addDays(1);

        // Set by QDate
        backend.setMinimumDate(minDate);
        backend.setMaximumDate(maxDate);

        QCOMPARE(backend.minimumDate(), minDate);
        QCOMPARE(backend.maximumDate(), maxDate);
        QCOMPARE(backend.minimumDateMsecs(), minDateMs);
        QCOMPARE(backend.maximumDateMsecs(), maxDateMs);

        QCOMPARE(minimumDateChangedSpy.count(), 1);
        QCOMPARE(maximumDateChangedSpy.count(), 1);
        QCOMPARE(minimumDateMsecsChangedSpy.count(), 1);
        QCOMPARE(maximumDateMsecsChangedSpy.count(), 1);
        QCOMPARE(validDateChangedSpy.count(), 2); // Changes per each min/max date set

        // Reset and try when setting by MSecs
        backend.setMinimumDate({});
        backend.setMaximumDate({});
        backend.setMinimumDateMsecs(minDateMs);
        backend.setMaximumDateMsecs(maxDateMs);

        QCOMPARE(backend.minimumDate(), minDate);
        QCOMPARE(backend.maximumDate(), maxDate);
        QCOMPARE(backend.minimumDateMsecs(), minDateMs);
        QCOMPARE(backend.maximumDateMsecs(), maxDateMs);

        QCOMPARE(minimumDateChangedSpy.count(), 3);
        QCOMPARE(maximumDateChangedSpy.count(), 3);
        QCOMPARE(minimumDateMsecsChangedSpy.count(), 3);
        QCOMPARE(maximumDateMsecsChangedSpy.count(), 3);
        QCOMPARE(validDateChangedSpy.count(), 6);

        // Since we default to the current date, the date should be valid
        QVERIFY(backend.validDate());

        // Now try with invalid dates
        backend.setDate(invalidMinDate);
        QVERIFY(!backend.validDate());
        QCOMPARE(validDateChangedSpy.count(), 7);

        backend.setDate(invalidMaxDate);
        QVERIFY(!backend.validDate());
        QCOMPARE(validDateChangedSpy.count(), 8);
    }

    void testDateSettingMethods()
    {
        Quick::DateFieldBackend backend;
        backend._dateFormat = dateStringFormat;

        QSignalSpy dateChangedSpy(&backend, &Quick::DateFieldBackend::dateChanged);
        QSignalSpy dateMsecsChangedSpy(&backend, &Quick::DateFieldBackend::dateMsecsChanged);
        QSignalSpy dateStringChangedSpy(&backend, &Quick::DateFieldBackend::dateStringChanged);

        const auto testDate = QDate::currentDate().addDays(800);
        const auto testDateMsecs = testDate.startOfDay(QTimeZone::utc()).toMSecsSinceEpoch();
        const auto testDateString = testDate.toString(dateStringFormat);

        backend.setDate(testDate);
        QCOMPARE(backend.date(), testDate);
        QCOMPARE(dateChangedSpy.count(), 1);
        QCOMPARE(dateMsecsChangedSpy.count(), 1);
        QCOMPARE(dateStringChangedSpy.count(), 1);

        backend.setDate({});
        QVERIFY(backend.date() != testDate);
        QCOMPARE(dateChangedSpy.count(), 2);
        QCOMPARE(dateMsecsChangedSpy.count(), 2);
        QCOMPARE(dateStringChangedSpy.count(), 2);

        backend.setDateMsecs(testDateMsecs);
        QCOMPARE(backend.date(), testDate);
        QCOMPARE(dateChangedSpy.count(), 3);
        QCOMPARE(dateMsecsChangedSpy.count(), 3);
        QCOMPARE(dateStringChangedSpy.count(), 3);

        backend.setDate({});
        QVERIFY(backend.date() != testDate);
        QCOMPARE(dateChangedSpy.count(), 4);
        QCOMPARE(dateMsecsChangedSpy.count(), 4);
        QCOMPARE(dateStringChangedSpy.count(), 4);

        backend.setDateString(testDateString);
        QCOMPARE(backend.date(), testDate);
        QCOMPARE(dateChangedSpy.count(), 5);
        QCOMPARE(dateMsecsChangedSpy.count(), 5);
        QCOMPARE(dateStringChangedSpy.count(), 5);
    }
};

QTEST_MAIN(TestDateFieldBackend)
#include "testdatefieldbackend.moc"
