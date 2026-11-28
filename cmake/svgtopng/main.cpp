/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QGuiApplication>
#include <QImage>
#include <QLoggingCategory>
#include <QPainter>
#include <QSvgRenderer>

#include <cstdlib>

Q_LOGGING_CATEGORY(lcSvgToPng, "nextcloud.svgtopng", QtInfoMsg)

/**
 * @brief Renders an SVG file to a square PNG file with Qt Svg.
 *
 * Usage: svgtopng <size> <input.svg> <output.png>
 *
 * The build runs this tool to make the raster icons. It exits with a failure code and writes no file when it cannot do so.
 */
int main(int argc, char *argv[])
{
    // Rendering to an image needs no display, and build machines often have none.
    qputenv("QT_QPA_PLATFORM", "offscreen");

    // Without a terminal, Qt may log to the system log or to the debugger only, where the build does not read the messages.
    qputenv("QT_FORCE_STDERR_LOGGING", "1");

    // Qt Svg lays out text with the font database, which needs a GUI application.
    const auto application = QGuiApplication(argc, argv);

    constexpr auto expectedArgumentCount = 4;
    const auto arguments = QGuiApplication::arguments();

    if (arguments.size() != expectedArgumentCount) {
        qCCritical(lcSvgToPng) << "Usage: svgtopng <size> <input.svg> <output.png>";
        return EXIT_FAILURE;
    }

    auto isNumber = false;
    const auto size = arguments.at(1).toInt(&isNumber);

    if (!isNumber || size <= 0) {
        qCCritical(lcSvgToPng) << "The size must be a positive number of pixels:" << arguments.at(1);
        return EXIT_FAILURE;
    }

    const auto &inputPath = arguments.at(2);
    auto renderer = QSvgRenderer(inputPath);

    if (!renderer.isValid()) {
        qCCritical(lcSvgToPng) << "Could not read the SVG file:" << inputPath;
        return EXIT_FAILURE;
    }

    // The same steps as in Theme::themeIcon. The SVG fills the whole image, which stretches one that is not square.
    auto image = QImage(size, size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    auto painter = QPainter(&image);
    renderer.render(&painter);
    painter.end();

    const auto &outputPath = arguments.at(3);

    if (!image.save(outputPath, "PNG")) {
        qCCritical(lcSvgToPng) << "Could not write the PNG file:" << outputPath;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
