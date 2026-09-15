/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QList>
#include <QStringView>
#include <QTest>
#include <QXmlStreamReader>

namespace {

QString customStateIconDirectory()
{
    return QDir(QStringLiteral(NEXTCLOUD_SOURCE_DIR)).filePath(QStringLiteral("theme/cfapishellext_custom_states"));
}

bool isColor(const QStringView value, const Qt::GlobalColor expected)
{
    const auto color = QColor(value.toString());
    return color.isValid() && color == QColor(expected);
}

bool isLightPixel(const QRgb pixel)
{
    return qAlpha(pixel) >= 128 && qRed(pixel) >= 230 && qGreen(pixel) >= 230 && qBlue(pixel) >= 230;
}

bool isDarkPixel(const QRgb pixel)
{
    return qAlpha(pixel) >= 128 && qRed(pixel) <= 25 && qGreen(pixel) <= 25 && qBlue(pixel) <= 25;
}

}

class TestCustomStateIcons : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSvgContrast_data()
    {
        QTest::addColumn<QString>("fileName");

        QTest::newRow("locked") << QStringLiteral("0-locked.svg");
        QTest::newRow("shared") << QStringLiteral("1-shared.svg");
    }

    void testSvgContrast()
    {
        QFETCH(QString, fileName);

        QFile file(QDir(customStateIconDirectory()).filePath(fileName));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));

        auto xml = QXmlStreamReader(&file);
        auto shapeCount = 0;

        while (!xml.atEnd()) {
            xml.readNext();

            if (!xml.isStartElement()) {
                continue;
            }

            const auto elementName = xml.name();
            const auto isShape = elementName == QStringLiteral("circle")
                || elementName == QStringLiteral("ellipse")
                || elementName == QStringLiteral("line")
                || elementName == QStringLiteral("path")
                || elementName == QStringLiteral("polygon")
                || elementName == QStringLiteral("polyline")
                || elementName == QStringLiteral("rect");

            if (!isShape) {
                continue;
            }

            ++shapeCount;
            QVERIFY2(elementName == QStringLiteral("path"),
                qPrintable(fileName + QStringLiteral(" must use one uniform path")));

            const auto attributes = xml.attributes();
            QVERIFY2(isColor(attributes.value(QStringLiteral("fill")), Qt::white),
                qPrintable(fileName + QStringLiteral(" paths must use a light fill")));
            QVERIFY2(isColor(attributes.value(QStringLiteral("stroke")), Qt::black),
                qPrintable(fileName + QStringLiteral(" paths must use a dark outline")));
        }

        QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
        QCOMPARE(shapeCount, 1);
    }

    void testRasterContrast_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<int>("expectedSize");

        const auto directory = QDir(customStateIconDirectory());
        const auto sizes = QList<int>{24, 32, 40, 48, 64, 128, 256, 512, 1024};
        const auto stateNames = QStringList{QStringLiteral("0-locked"), QStringLiteral("1-shared")};
        auto expectedFiles = QStringList{};

        for (const auto &stateName : stateNames) {
            for (const auto size : sizes) {
                const auto fileName = QStringLiteral("%1-%2.png").arg(size).arg(stateName);
                expectedFiles.append(fileName);

                const auto rowName = fileName.toUtf8();
                QTest::newRow(rowName.constData()) << fileName << size;
            }
        }

        expectedFiles.sort();
        const auto pngFiles = directory.entryList(
            QStringList{QStringLiteral("*.png")}, QDir::Files | QDir::Readable, QDir::Name);
        QCOMPARE(pngFiles, expectedFiles);
    }

    void testRasterContrast()
    {
        QFETCH(QString, fileName);
        QFETCH(int, expectedSize);

        auto image = QImage(QDir(customStateIconDirectory()).filePath(fileName));
        QVERIFY2(!image.isNull(), qPrintable(fileName + QStringLiteral(" must be a readable image")));
        QCOMPARE(image.size(), QSize(expectedSize, expectedSize));

        image = image.convertToFormat(QImage::Format_ARGB32);

        auto hasLightPixel = false;
        auto hasDarkPixel = false;
        auto hasUnexpectedColor = false;

        for (auto y = 0; y < image.height() && !hasUnexpectedColor; ++y) {
            const auto pixels = reinterpret_cast<const QRgb *>(image.constScanLine(y));

            for (auto x = 0; x < image.width(); ++x) {
                const auto pixel = pixels[x];

                if (qAlpha(pixel) == 0) {
                    continue;
                }

                hasUnexpectedColor = qRed(pixel) != qGreen(pixel) || qGreen(pixel) != qBlue(pixel);

                if (hasUnexpectedColor) {
                    break;
                }

                if (qAlpha(pixel) < 128) {
                    continue;
                }

                hasLightPixel = hasLightPixel || isLightPixel(pixel);
                hasDarkPixel = hasDarkPixel || isDarkPixel(pixel);
            }
        }

        QVERIFY2(hasLightPixel, qPrintable(fileName + QStringLiteral(" must retain a light interior")));
        QVERIFY2(hasDarkPixel, qPrintable(fileName + QStringLiteral(" must retain a dark outline")));
        QVERIFY2(!hasUnexpectedColor, qPrintable(fileName + QStringLiteral(" must contain only grayscale pixels")));

        if (fileName.endsWith(QStringLiteral("-1-shared.png"))) {
            const auto headCenter = image.pixel(expectedSize * 3 / 8, expectedSize / 3);
            const auto bodyCenter = image.pixel(expectedSize * 3 / 8, expectedSize * 7 / 10);
            QVERIFY2(isLightPixel(headCenter), qPrintable(fileName + QStringLiteral(" must retain a filled head")));
            QVERIFY2(isLightPixel(bodyCenter), qPrintable(fileName + QStringLiteral(" must retain a filled body")));
        }
    }
};

QTEST_APPLESS_MAIN(TestCustomStateIcons)
#include "testcustomstateicons.moc"
