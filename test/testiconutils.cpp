/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include <QTest>

#include "theme.h"
#include "iconutils.h"

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
    void testDrawSvgWithCustomFillColor()
    {
        const QString blackSvgDirPath{QString{OCC::Theme::themePrefix} + QStringLiteral("black")};
        const QDir blackSvgDir(blackSvgDirPath);
        const QStringList blackImages = blackSvgDir.entryList(QStringList("*.svg"));

        if (!blackImages.isEmpty()) {
            QVERIFY(!OCC::Ui::IconUtils::drawSvgWithCustomFillColor(blackSvgDirPath + QLatin1Char('/') + blackImages.at(0), QColorConstants::Svg::red).isNull());
        }

        if (!blackImages.isEmpty()) {
            QVERIFY(!OCC::Ui::IconUtils::drawSvgWithCustomFillColor(blackSvgDirPath + QLatin1Char('/') + blackImages.at(0), QColorConstants::Svg::green).isNull());
        }

        const QString whiteSvgDirPath{QString{OCC::Theme::themePrefix} + QStringLiteral("white")};
        const QDir whiteSvgDir(whiteSvgDirPath);
        const QStringList whiteImages = whiteSvgDir.entryList(QStringList("*.svg"));

        if (!whiteImages.isEmpty()) {
            QVERIFY(!OCC::Ui::IconUtils::drawSvgWithCustomFillColor(whiteSvgDirPath + QLatin1Char('/') + whiteImages.at(0), QColorConstants::Svg::blue).isNull());
        }
    }

    void testCreateSvgPixmapWithCustomColor()
    {
        const QDir blackSvgDir(QString(QString{OCC::Theme::themePrefix}) + QStringLiteral("black"));
        const QStringList blackImages = blackSvgDir.entryList(QStringList("*.svg"));

        if (!blackImages.isEmpty()) {
            QVERIFY(!OCC::Ui::IconUtils::createSvgPixmapWithCustomColor(blackImages.at(0), QColorConstants::Svg::red).isNull());
        }

        if (!blackImages.isEmpty()) {
            QVERIFY(!OCC::Ui::IconUtils::createSvgPixmapWithCustomColor(blackImages.at(0), QColorConstants::Svg::green).isNull());
        }

        const QDir whiteSvgDir(QString(QString{OCC::Theme::themePrefix}) + QStringLiteral("white"));
        const QStringList whiteImages = whiteSvgDir.entryList(QStringList("*.svg"));
        
        if (!whiteImages.isEmpty()) {
            QVERIFY(!OCC::Ui::IconUtils::createSvgPixmapWithCustomColor(whiteImages.at(0), QColorConstants::Svg::blue).isNull());
        }
    }

    void testPixmapForBackground()
    {
        const QDir blackSvgDir(QString(QString{OCC::Theme::themePrefix}) + QStringLiteral("black"));
        const QStringList blackImages = blackSvgDir.entryList(QStringList("*.svg"));

        const QDir whiteSvgDir(QString(QString{OCC::Theme::themePrefix}) + QStringLiteral("white"));
        const QStringList whiteImages = whiteSvgDir.entryList(QStringList("*.svg"));

        if (blackImages.size() > 0) {
            // white pixmap for dark background - should not fail
            QVERIFY(!OCC::Ui::IconUtils::pixmapForBackground(whiteImages.at(0), QColor("blue")).isNull());
        }

        if (whiteImages.size() > 0) {
            // black pixmap for bright background - should not fail
            QVERIFY(!OCC::Ui::IconUtils::pixmapForBackground(blackImages.at(0), QColor("yellow")).isNull());
        }
    }
};

QTEST_MAIN(TestIconUtils)
#include "testiconutils.moc"
