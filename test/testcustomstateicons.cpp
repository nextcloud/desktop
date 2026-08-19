/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
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

}

class TestCustomStateIcons : public QObject
{
    Q_OBJECT

private slots:
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
        auto hasLightFill = false;
        auto hasDarkStroke = false;

        while (!xml.atEnd()) {
            xml.readNext();

            if (!xml.isStartElement()) {
                continue;
            }

            const auto attributes = xml.attributes();
            hasLightFill = hasLightFill || isColor(attributes.value(QStringLiteral("fill")), Qt::white);
            hasDarkStroke = hasDarkStroke || isColor(attributes.value(QStringLiteral("stroke")), Qt::black);
        }

        QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
        QVERIFY2(hasLightFill, qPrintable(fileName + QStringLiteral(" must retain a light fill")));
        QVERIFY2(hasDarkStroke, qPrintable(fileName + QStringLiteral(" must retain a dark outline")));
    }

    void testRasterContrast_data()
    {
        QTest::addColumn<QString>("fileName");

        const auto directory = QDir(customStateIconDirectory());
        const auto lockedFiles = directory.entryList(
            QStringList{QStringLiteral("*-0-locked.png")},
            QDir::Files | QDir::Readable,
            QDir::Name);
        const auto sharedFiles = directory.entryList(
            QStringList{QStringLiteral("*-1-shared.png")},
            QDir::Files | QDir::Readable,
            QDir::Name);
        const auto pngFiles = lockedFiles + sharedFiles;

        QCOMPARE(lockedFiles.size(), sharedFiles.size());
        QCOMPARE(pngFiles.size(), 18);

        for (const auto &fileName : pngFiles) {
            const auto rowName = fileName.toUtf8();
            QTest::newRow(rowName.constData()) << fileName;
        }
    }

    void testRasterContrast()
    {
        QFETCH(QString, fileName);

        auto image = QImage(QDir(customStateIconDirectory()).filePath(fileName));
        QVERIFY2(!image.isNull(), qPrintable(fileName + QStringLiteral(" must be a readable image")));

        image = image.convertToFormat(QImage::Format_ARGB32);

        auto hasLightPixel = false;
        auto hasDarkPixel = false;

        for (auto y = 0; y < image.height() && !(hasLightPixel && hasDarkPixel); ++y) {
            const auto pixels = reinterpret_cast<const QRgb *>(image.constScanLine(y));

            for (auto x = 0; x < image.width(); ++x) {
                const auto pixel = pixels[x];

                if (qAlpha(pixel) < 128) {
                    continue;
                }

                hasLightPixel = hasLightPixel
                    || (qRed(pixel) >= 230 && qGreen(pixel) >= 230 && qBlue(pixel) >= 230);
                hasDarkPixel = hasDarkPixel
                    || (qRed(pixel) <= 25 && qGreen(pixel) <= 25 && qBlue(pixel) <= 25);

                if (hasLightPixel && hasDarkPixel) {
                    break;
                }
            }
        }

        QVERIFY2(hasLightPixel, qPrintable(fileName + QStringLiteral(" must retain a light interior")));
        QVERIFY2(hasDarkPixel, qPrintable(fileName + QStringLiteral(" must retain a dark outline")));
    }
};

QTEST_APPLESS_MAIN(TestCustomStateIcons)
#include "testcustomstateicons.moc"
