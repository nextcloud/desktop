/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include "uiscreenshots/uiscreenshotmode.h"

#include <QTest>

using namespace OCC::UiScreenshots;

class UiScreenshotModeTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesSupportedValues_data()
    {
        QTest::addColumn<QByteArray>("value");
        QTest::addColumn<int>("expectedPhase");

        QTest::newRow("normal") << QByteArray{} << static_cast<int>(Phase::None);
        QTest::newRow("qml") << QByteArrayLiteral("qml") << static_cast<int>(Phase::Qml);
        QTest::newRow("legacy qml") << QByteArrayLiteral("1") << static_cast<int>(Phase::Qml);
        QTest::newRow("native") << QByteArrayLiteral("native") << static_cast<int>(Phase::Native);
    }

    void parsesSupportedValues()
    {
        QFETCH(QByteArray, value);
        QFETCH(int, expectedPhase);

        const auto parsed = parsePhase(value);
        QCOMPARE_EQ(static_cast<int>(parsed.phase), expectedPhase);
        QVERIFY(parsed.error.isEmpty());
    }

    void rejectsUnsupportedValue()
    {
        const auto parsed = parsePhase(QByteArrayLiteral("Native"));

        QCOMPARE_EQ(parsed.phase, Phase::Invalid);
        QVERIFY(parsed.error.contains(QStringLiteral("Native")));
    }
};

QTEST_GUILESS_MAIN(UiScreenshotModeTest)

#include "UiScreenshotModeTest.moc"
