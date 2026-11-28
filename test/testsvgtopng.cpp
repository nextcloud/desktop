/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QColor>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <cstdlib>

using namespace Qt::StringLiterals;

/**
 * @brief Runs the svgtopng tool of the build and checks its exit code and the PNG file it writes.
 */
class TestSvgToPng : public QObject
{
    Q_OBJECT

private:
    static constexpr auto didNotFinish = -1;
    static inline const auto smallSize = u"16"_s;
    static inline const auto squareSvg =
        R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 4 4"><rect x="1" y="1" width="2" height="2" fill="#f00"/></svg>)"_ba;
    static inline const auto wideSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 18 16"><rect width="18" height="16" fill="#f00"/></svg>)"_ba;

    /** @brief Runs the tool and returns its exit code, or didNotFinish when it crashed, hung or did not start. */
    static int run(const QStringList &arguments)
    {
        auto process = QProcess();
        process.start(QString::fromUtf8(SVGTOPNG_EXECUTABLE), arguments);

        if (!process.waitForFinished()) {
            process.kill();
            process.waitForFinished();
            return didNotFinish;
        }

        return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : didNotFinish;
    }

    static bool write(const QString &path, const QByteArray &content)
    {
        auto file = QFile(path);
        return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
    }

private Q_SLOTS:
    void testRendersAtRequestedSize()
    {
        constexpr auto size = 64;
        const auto directory = QTemporaryDir();
        const auto svgPath = directory.filePath(u"square.svg"_s);
        const auto pngPath = directory.filePath(u"square.png"_s);
        QVERIFY(write(svgPath, squareSvg));

        QCOMPARE(run({QString::number(size), svgPath, pngPath}), EXIT_SUCCESS);

        const auto image = QImage(pngPath);
        QCOMPARE(image.size(), QSize(size, size));
        QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
        QCOMPARE(image.pixelColor(size / 2, size / 2), QColor(Qt::red));
    }

    void testStretchesSvgThatIsNotSquare()
    {
        // The build expects a square raster icon without transparent bars, also from an SVG file that is wider than high.
        constexpr auto size = 32;
        const auto directory = QTemporaryDir();
        const auto svgPath = directory.filePath(u"wide.svg"_s);
        const auto pngPath = directory.filePath(u"wide.png"_s);
        QVERIFY(write(svgPath, wideSvg));

        QCOMPARE(run({QString::number(size), svgPath, pngPath}), EXIT_SUCCESS);

        const auto image = QImage(pngPath);
        QCOMPARE(image.size(), QSize(size, size));
        QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(image.pixelColor(size - 1, size - 1), QColor(Qt::red));
    }

    void testFailsWithoutOutput_data()
    {
        QTest::addColumn<QStringList>("leadingArguments");
        QTest::addColumn<QByteArray>("svg");

        QTest::newRow("size of zero") << QStringList{u"0"_s} << squareSvg;
        QTest::newRow("negative size") << QStringList{u"-16"_s} << squareSvg;
        QTest::newRow("size that is not a number") << QStringList{u"large"_s} << squareSvg;
        QTest::newRow("no size") << QStringList{} << squareSvg;
        QTest::newRow("one argument too many") << QStringList{smallSize, smallSize} << squareSvg;
        QTest::newRow("empty SVG file") << QStringList{smallSize} << QByteArray();
        QTest::newRow("file that is not SVG") << QStringList{smallSize} << "this is not SVG"_ba;
        QTest::newRow("truncated SVG file") << QStringList{smallSize} << squareSvg.first(squareSvg.size() / 2);
    }

    void testFailsWithoutOutput()
    {
        QFETCH(QStringList, leadingArguments);
        QFETCH(QByteArray, svg);

        const auto directory = QTemporaryDir();
        const auto svgPath = directory.filePath(u"input.svg"_s);
        const auto pngPath = directory.filePath(u"output.png"_s);
        QVERIFY(write(svgPath, svg));

        QCOMPARE(run(leadingArguments + QStringList{svgPath, pngPath}), EXIT_FAILURE);
        QVERIFY2(!QFile::exists(pngPath), "a failed run must not leave a PNG file");
    }

    void testFailsForMissingSvgFile()
    {
        const auto directory = QTemporaryDir();
        const auto pngPath = directory.filePath(u"output.png"_s);

        QCOMPARE(run({smallSize, directory.filePath(u"missing.svg"_s), pngPath}), EXIT_FAILURE);
        QVERIFY2(!QFile::exists(pngPath), "a failed run must not leave a PNG file");
    }

    void testFailsWhenPngFileCannotBeWritten()
    {
        const auto directory = QTemporaryDir();
        const auto svgPath = directory.filePath(u"square.svg"_s);
        QVERIFY(write(svgPath, squareSvg));

        QCOMPARE(run({smallSize, svgPath, directory.filePath(u"missing-directory/output.png"_s)}), EXIT_FAILURE);
    }
};

QTEST_GUILESS_MAIN(TestSvgToPng)
#include "testsvgtopng.moc"
