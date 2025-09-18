/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>

#include "theme.h"
#include "iconutils.h"
#include "logger.h"

#include <QStandardPaths>

class TestIconUtils : public QObject
{
    Q_OBJECT

public:
    TestIconUtils()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
    }

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testDrawSvgWithCustomFillColor()
    {
        const QString blackSvgDirPath{QString{OCC::Theme::themePrefix} + QStringLiteral("black")};
        const QDir blackSvgDir(blackSvgDirPath);
        const QStringList blackImages = blackSvgDir.entryList(QStringList("*.svg"));

        Q_ASSERT(!blackImages.isEmpty());

        QVERIFY(!OCC::Ui::IconUtils::drawSvgWithCustomFillColor(blackSvgDirPath + QStringLiteral("/") + blackImages.at(0), QColorConstants::Svg::red).isNull());

        QVERIFY(!OCC::Ui::IconUtils::drawSvgWithCustomFillColor(blackSvgDirPath + QStringLiteral("/") + blackImages.at(0), QColorConstants::Svg::green).isNull());

        const QString whiteSvgDirPath{QString{OCC::Theme::themePrefix} + QStringLiteral("white")};
        const QDir whiteSvgDir(whiteSvgDirPath);
        const QStringList whiteImages = whiteSvgDir.entryList(QStringList("*.svg"));

        Q_ASSERT(!whiteImages.isEmpty());

        QVERIFY(!OCC::Ui::IconUtils::drawSvgWithCustomFillColor(whiteSvgDirPath + QStringLiteral("/") + whiteImages.at(0), QColorConstants::Svg::blue).isNull());
    }

    void testCreateSvgPixmapWithCustomColor()
    {
        const QDir blackSvgDir(QString(QString{OCC::Theme::themePrefix}) + QStringLiteral("black"));
        const QStringList blackImages = blackSvgDir.entryList(QStringList("*.svg"));

        QVERIFY(!blackImages.isEmpty());

        QVERIFY(!OCC::Ui::IconUtils::createSvgImageWithCustomColor(blackImages.at(0), QColorConstants::Svg::red).isNull());

        QVERIFY(!OCC::Ui::IconUtils::createSvgImageWithCustomColor(blackImages.at(0), QColorConstants::Svg::green).isNull());

        const QDir whiteSvgDir(QString(QString{OCC::Theme::themePrefix}) + QStringLiteral("white"));
        const QStringList whiteImages = whiteSvgDir.entryList(QStringList("*.svg"));
        
        QVERIFY(!whiteImages.isEmpty());

        QVERIFY(!OCC::Ui::IconUtils::createSvgImageWithCustomColor(whiteImages.at(0), QColorConstants::Svg::blue).isNull());
    }

    void testPixmapForBackground()
    {
        const QDir blackSvgDir(QString(QString{OCC::Theme::themePrefix}) + QStringLiteral("black"));
        const QStringList blackImages = blackSvgDir.entryList(QStringList("*.svg"));

        const QDir whiteSvgDir(QString(QString{OCC::Theme::themePrefix}) + QStringLiteral("white"));
        const QStringList whiteImages = whiteSvgDir.entryList(QStringList("*.svg"));

        QVERIFY(!blackImages.isEmpty());

        QVERIFY(!OCC::Ui::IconUtils::pixmapForBackground(whiteImages.at(0), QColor("blue")).isNull());

        QVERIFY(!whiteImages.isEmpty());

        QVERIFY(!OCC::Ui::IconUtils::pixmapForBackground(blackImages.at(0), QColor("yellow")).isNull());
    }
};

QTEST_MAIN(TestIconUtils)
#include "testiconutils.moc"
