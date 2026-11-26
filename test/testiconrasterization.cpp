/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPoint>
#include <QSvgRenderer>
#include <QTest>
#include <QXmlStreamReader>

using namespace Qt::StringLiterals;

/**
 * @brief Checks that Qt Svg draws the four icons it once drew wrongly.
 *
 * Qt Svg skips the SVG features it does not implement without reporting an error, so only the picture shows the problem.
 */
class TestIconRasterization : public QObject
{
    Q_OBJECT

private:
    static constexpr auto opaqueAlpha = 230;
    static constexpr auto whiteLightness = 230;
    static constexpr auto darkLightness = 25;

    //! A pixel differs when a colour channel is off by more than 30 % of its range, which ignores most antialiasing.
    static constexpr auto channelTolerance = 255 * 3 / 10;

    //! With Qt 6.11, at most 0.1 % of the pixels differ for a correct icon and at least 8 % for an outline of twice the width.
    static constexpr auto maximumDifferingPixelShare = 0.02;

    static QString themePath(const QString &relativePath)
    {
        return QDir(QString::fromUtf8(NEXTCLOUD_SOURCE_DIR)).filePath("theme/"_L1 + relativePath);
    }

    /** @brief Draws an SVG file stretched over a square image, as Theme::themeIcon does. Returns a null image for an unreadable file. */
    static QImage render(const QString &svgPath, const int size)
    {
        auto renderer = QSvgRenderer(svgPath);

        if (!renderer.isValid()) {
            return {};
        }

        auto image = QImage(size, size, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        auto painter = QPainter(&image);
        renderer.render(&painter);
        return image;
    }

    static bool isWhite(const QColor &color)
    {
        return color.alpha() >= opaqueAlpha && color.lightness() >= whiteLightness;
    }

    static bool isDark(const QColor &color)
    {
        return color.alpha() >= opaqueAlpha && color.lightness() <= darkLightness;
    }

    static bool isBlue(const QColor &color)
    {
        return color.alpha() >= opaqueAlpha && color.blue() > color.red() && color.blue() > color.green();
    }

    static bool isYellow(const QColor &color)
    {
        return color.alpha() >= opaqueAlpha && color.red() > color.blue() && color.green() > color.blue();
    }

    static bool differs(const QRgb first, const QRgb second)
    {
        return qAbs(qAlpha(first) - qAlpha(second)) > channelTolerance || qAbs(qRed(first) - qRed(second)) > channelTolerance
            || qAbs(qGreen(first) - qGreen(second)) > channelTolerance || qAbs(qBlue(first) - qBlue(second)) > channelTolerance;
    }

    /** @brief Returns the share of pixels that differ between two images of the same size. */
    static double differingPixelShare(const QImage &image, const QImage &reference)
    {
        // Premultiplied colours keep the arbitrary colour of a transparent pixel out of the comparison.
        const auto first = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const auto second = reference.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        auto differingPixels = 0;

        for (auto y = 0; y < first.height(); ++y) {
            const auto firstLine = reinterpret_cast<const QRgb *>(first.constScanLine(y));
            const auto secondLine = reinterpret_cast<const QRgb *>(second.constScanLine(y));

            for (auto x = 0; x < first.width(); ++x) {
                if (differs(firstLine[x], secondLine[x])) {
                    ++differingPixels;
                }
            }
        }

        return static_cast<double>(differingPixels) / (first.width() * first.height());
    }

private Q_SLOTS:
    void testCustomStateIconMatchesRaster_data()
    {
        QTest::addColumn<QString>("rasterName");

        const auto rasterNames = QDir(themePath(u"cfapishellext_custom_states"_s)).entryList({u"*.png"_s}, QDir::Files);

        for (const auto &rasterName : rasterNames) {
            QTest::addRow("%s", qUtf8Printable(rasterName)) << rasterName;
        }
    }

    void testCustomStateIconMatchesRaster()
    {
        // Qt Test calls this function once without data when it found no raster icon, and QFETCH then aborts the program.
        QVERIFY2(QTest::currentDataTag() != nullptr, "found no raster icon to compare");
        QFETCH(QString, rasterName);

        // The raster icon <size>-<name>.png is drawn from <name>.svg.
        const auto directory = QDir(themePath(u"cfapishellext_custom_states"_s));
        const auto raster = QImage(directory.filePath(rasterName));
        QVERIFY2(!raster.isNull(), "the raster icon must be a readable image");
        const auto svgName = rasterName.section(u'-', 1).replace(".png"_L1, ".svg"_L1);
        const auto rendered = render(directory.filePath(svgName), raster.width());
        QVERIFY(!rendered.isNull());
        QCOMPARE(rendered.size(), raster.size());

        const auto share = differingPixelShare(rendered, raster);
        QVERIFY2(share <= maximumDifferingPixelShare, qPrintable(u"%1 % of the pixels differ from the raster icon"_s.arg(share * 100)));
    }

    void testMacOSAppIconShowsLogo()
    {
        // The image is as large as the viewBox of the icon, so a position here is a position in the SVG file.
        constexpr auto viewBoxSize = 1024;
        const auto middleRing = QPoint(512, 390);
        const auto leftRing = QPoint(292, 442);
        const auto rightRing = QPoint(732, 442);
        const auto middleRingHole = QPoint(512, 512);
        const auto outsideOfIcon = QPoint(16, 16);

        const auto image = render(themePath(u"colored/Nextcloud-macOS-icon.svg"_s), viewBoxSize);
        QVERIFY(!image.isNull());

        QVERIFY2(isWhite(image.pixelColor(middleRing)), "the middle ring of the logo must be white");
        QVERIFY2(isWhite(image.pixelColor(leftRing)), "the left ring of the logo must be white");
        QVERIFY2(isWhite(image.pixelColor(rightRing)), "the right ring of the logo must be white");
        QVERIFY2(isBlue(image.pixelColor(middleRingHole)), "the background must show through the middle ring");
        QCOMPARE(image.pixelColor(outsideOfIcon).alpha(), 0);
    }

    void testMacOSAppIconHasNoPercentLength()
    {
        QFile file(themePath(u"colored/Nextcloud-macOS-icon.svg"_s));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));

        auto xml = QXmlStreamReader(&file);

        while (!xml.atEnd()) {
            xml.readNext();

            // Qt Svg resolves a percent length against the viewBox only for the size of the root element.
            if (!xml.isStartElement() || xml.name() == "svg"_L1) {
                continue;
            }

            // Stricter than Qt Svg needs: it reads the offset of a gradient stop in percent correctly, which is allowed here,
            // and the vector of a gradient as well, unless gradientUnits is userSpaceOnUse.
            for (const auto &attribute : xml.attributes()) {
                QVERIFY2(attribute.name() == "offset"_L1 || !attribute.value().contains(u'%'),
                         qPrintable(u"%1 has a percent in its %2"_s.arg(xml.name(), attribute.name())));
            }
        }

        QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
    }

    void testWindowsFolderIconHasNoStrayShape()
    {
        // The image is as large as the viewBox of the icon, so a position here is a position in the SVG file.
        constexpr auto viewBoxSize = 300;
        const auto folderBack = QPoint(150, 60);
        const auto logoRing = QPoint(187, 183);
        const auto logoBackground = QPoint(187, 203);
        const auto besideFolder = QPoint(240, 160);

        const auto image = render(themePath(u"colored/icons/Nextcloud-icon-win-folder.svg"_s), viewBoxSize);
        QVERIFY(!image.isNull());

        // No part of the icon is dark, but Qt Svg draws the content of a <clipPath> as a black shape.
        auto darkPixels = 0;

        for (auto y = 0; y < image.height(); ++y) {
            for (auto x = 0; x < image.width(); ++x) {
                if (isDark(image.pixelColor(x, y))) {
                    ++darkPixels;
                }
            }
        }

        QCOMPARE(darkPixels, 0);
        QVERIFY2(isYellow(image.pixelColor(folderBack)), "the back of the folder must be yellow");
        QVERIFY2(isWhite(image.pixelColor(logoRing)), "the logo must be white");
        QVERIFY2(isBlue(image.pixelColor(logoBackground)), "the logo must sit on a blue disc");
        QCOMPARE(image.pixelColor(besideFolder).alpha(), 0);
    }
};

QTEST_MAIN(TestIconRasterization)
#include "testiconrasterization.moc"
