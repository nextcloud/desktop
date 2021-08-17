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
    void testPixmapForBackground()
    {
        const QDir blackSvgDir(QString(OCC::Theme::themePrefix) + QStringLiteral("black"));
        const QStringList blackImages = blackSvgDir.entryList(QStringList("*.svg"));

        const QDir whiteSvgDir(QString(OCC::Theme::themePrefix) + QStringLiteral("white"));
        const QStringList whiteImages = whiteSvgDir.entryList(QStringList("*.svg"));

        if (blackImages.size() > 0) {
            // white pixmap for dark background - should not fail
            QVERIFY(!OCC::Ui::IconUtils::pixmapForBackground(whiteImages.at(0), QColor("blue")).isNull());
        }

        if (whiteImages.size() > 0) {
            // black pixmap for bright background - should not fail
            QVERIFY(!OCC::Ui::IconUtils::pixmapForBackground(blackImages.at(0), QColor("yellow")).isNull());
        }

        const auto blackImagesExclusive = QSet<QString>(blackImages.begin(), blackImages.end()).subtract(QSet<QString>(whiteImages.begin(), whiteImages.end()));
        const auto whiteImagesExclusive = QSet<QString>(whiteImages.begin(), whiteImages.end()).subtract(QSet<QString>(blackImages.begin(), blackImages.end()));

        if (blackImagesExclusive != whiteImagesExclusive) {
            // black pixmap for dark background - should fail as we don't have this image in black
            QVERIFY(OCC::Ui::IconUtils::pixmapForBackground(blackImagesExclusive.values().at(0), QColor("blue")).isNull());

            // white pixmap for bright background - should fail as we don't have this image in white
            QVERIFY(OCC::Ui::IconUtils::pixmapForBackground(whiteImagesExclusive.values().at(0), QColor("yellow")).isNull());
        }
    }
};

QTEST_MAIN(TestIconUtils)
#include "testiconutils.moc"
