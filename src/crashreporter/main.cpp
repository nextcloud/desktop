/*
 * Copyright (C) by Dominik Schmidt <domme@tomahawk-player.org>
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


#include "CrashReporterConfig.h"

#include <libcrashreporter-gui/CrashReporter.h>

#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifdef Q_OS_WIN
    // The Windows style still has pixelated elements with Qt 5.6,
    // it's recommended to use the Fusion style in this case, even
    // though it looks slightly less native. Check here after the
    // QApplication was constructed, but before any QWidget is
    // constructed.
    if (qFuzzyCompare(app.devicePixelRatio(), 1)) {
        QApplication::setStyle(QStringLiteral("fusion"));
    }
#endif // Q_OS_WIN

    if (app.arguments().size() != 2) {
        qDebug() << "You need to pass the .dmp file path as only argument";
        return 1;
    }

    // TODO: install socorro ....
    CrashReporter reporter(QUrl(QStringLiteral(CRASHREPORTER_SUBMIT_URL)), app.arguments());

#ifdef CRASHREPORTER_ICON
    reporter.setLogo(QPixmap(QStringLiteral(CRASHREPORTER_ICON)));
#endif
    reporter.setWindowTitle(QStringLiteral(CRASHREPORTER_PRODUCT_NAME));
    reporter.setText(QStringLiteral("<html><head/><body><p><span style=\" font-weight:600;\">Sorry!</span> " CRASHREPORTER_PRODUCT_NAME
                                    " crashed. Please tell us about it! " CRASHREPORTER_PRODUCT_NAME
                                    " has created an error report for you that can help improve the stability in the future. You can now send this report "
                                    "directly to the " CRASHREPORTER_PRODUCT_NAME " developers.</p></body></html>"));

    const QFileInfo crashLog(QDir::tempPath() + QStringLiteral("/" CRASHREPORTER_PRODUCT_NAME "-crash.log"));
    if (crashLog.exists()) {
        QFile inFile(crashLog.filePath());
        if (inFile.open(QFile::ReadOnly)) {
            reporter.setComment(QString::fromUtf8(inFile.readAll()));
        }
    }

    reporter.setReportData("BuildID", CRASHREPORTER_BUILD_ID);
    reporter.setReportData("ProductName", CRASHREPORTER_PRODUCT_NAME);
    reporter.setReportData("Version", CRASHREPORTER_VERSION_STRING);
    reporter.setReportData("ReleaseChannel", CRASHREPORTER_RELEASE_CHANNEL);

    //reporter.setReportData( "timestamp", QByteArray::number( QDateTime::currentDateTime().toTime_t() ) );


    // add parameters

    //            << Pair("InstallTime", "1357622062")
    //            << Pair("Theme", "classic/1.0")
    //            << Pair("Version", "30")
    //            << Pair("id", "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}")
    //            << Pair("Vendor", "Mozilla")
    //            << Pair("EMCheckCompatibility", "true")
    //            << Pair("Throttleable", "0")
    //            << Pair("URL", "http://code.google.com/p/crashme/")
    //            << Pair("version", "20.0a1")
    //            << Pair("CrashTime", "1357770042")
    //            << Pair("submitted_timestamp", "2013-01-09T22:21:18.646733+00:00")
    //            << Pair("buildid", "20130107030932")
    //            << Pair("timestamp", "1357770078.646789")
    //            << Pair("Notes", "OpenGL: NVIDIA Corporation -- GeForce 8600M GT/PCIe/SSE2 -- 3.3.0 NVIDIA 313.09 -- texture_from_pixmap\r\n")
    //            << Pair("StartupTime", "1357769913")
    //            << Pair("FramePoisonSize", "4096")
    //            << Pair("FramePoisonBase", "7ffffffff0dea000")
    //            << Pair("Add-ons", "%7B972ce4c6-7e08-4474-a285-3208198ce6fd%7D:20.0a1,crashme%40ted.mielczarek.org:0.4")
    //            << Pair("SecondsSinceLastCrash", "1831736")
    //            << Pair("ProductName", "WaterWolf")
    //            << Pair("legacy_processing", "0")
    //            << Pair("ProductID", "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}")

    ;

    // TODO:
    // send log
    //    QFile logFile( INSERT_FILE_PATH_HERE );
    //    logFile.open( QFile::ReadOnly );
    //    reporter.setReportData( "upload_file_miralllog", qCompress( logFile.readAll() ), "application/x-gzip", QFileInfo( INSERT_FILE_PATH_HERE ).fileName().toUtf8());
    //    logFile.close();

    reporter.show();

    return app.exec();
}
