/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include "common/filesystembase.h"
#include "updater/updater.h"
#include "updater/ocupdater.h"
#include "configfile.h"
#include "logger.h"
#include "filesystem.h"

using namespace Qt::StringLiterals;

#ifdef HAVE_QHTTPSERVER
#include <QHttpServer>
#include <QTcpServer>
#endif

using namespace OCC;

class TestUpdater : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testVersionToInt()
    {
        qint64 lowVersion = Updater::Helper::versionToInt(1,2,80,3000);
        QCOMPARE(Updater::Helper::stringVersionToInt("1.2.80.3000"), lowVersion);

        qint64 highVersion = Updater::Helper::versionToInt(99,2,80,3000);
        qint64 currVersion = Updater::Helper::currentVersionToInt();
        QVERIFY(currVersion > 0);
        QVERIFY(currVersion > lowVersion);
        QVERIFY(currVersion < highVersion);
    }

#ifdef HAVE_QHTTPSERVER
    void testUpdaterDownloadRedirect()
    {
        QTemporaryDir tempDir;
        ConfigFile::setConfDir(tempDir.path()); // we don't want to pollute the user's config file
        QVERIFY(tempDir.isValid());
        QDir dir(tempDir.path());

        // set up a download server that provides the version info and redirects the download request to e.g. some object storage
        QHttpServer httpServer;
        httpServer.route("/updateinfo.xml", [](const QHttpServerRequest &request, QHttpServerResponder &responder) -> void {
            auto downloadTarget = request.url();
            downloadTarget.setPath("/Nextcloud.msi");
            qInfo() << "redirecting to" << downloadTarget;

            QHttpHeaders headers;
            headers.append(QHttpHeaders::WellKnownHeader::ContentType, "application/xml"_ba);

            auto xmlResponse = "<?xml version=\"1.0\"?>\n<owncloudclient><version>600.0.0</version><versionstring>Nextcloud Client 600.0.0</versionstring><downloadurl>"_ba;
            xmlResponse.append(downloadTarget.toEncoded());
            xmlResponse.append("</downloadurl><web>https://nextcloud.com/install</web></owncloudclient>"_ba);

            responder.write(xmlResponse, headers);
        });
        httpServer.route("/Nextcloud.msi", [](QHttpServerResponder &responder) -> void {
            QHttpHeaders headers;
            headers.append(QHttpHeaders::WellKnownHeader::Location, "/storage/blob/42?signature=1234abcd&signatureVersion=2026-01-21"_ba);
            responder.write(""_ba, headers, QHttpServerResponder::StatusCode::Found);
        });

        bool redirectHit = false;
        httpServer.route("/storage/blob/42", [&redirectHit](QHttpServerResponder &responder) -> void {
            QHttpHeaders headers;
            headers.append(QHttpHeaders::WellKnownHeader::ContentType, "application/octet-stream"_ba);
            redirectHit = true;

            responder.write("This would be the installer"_ba, headers);
        });

        QTcpServer tcpServer;
        QVERIFY(tcpServer.listen(QHostAddress::LocalHost));
        QVERIFY(httpServer.bind(&tcpServer));
        const QString baseUrl = "http://%1:%2"_L1.arg(tcpServer.serverAddress().toString(), QString::number(tcpServer.serverPort()));
        qInfo() << "Listening on" << baseUrl;

        NSISUpdater updater(QUrl("%1/updateinfo.xml"_L1.arg(baseUrl)));
        QSignalSpy downloadAvailableSpy(&updater, &OCUpdater::newUpdateAvailable);
        updater.checkForUpdate();
        downloadAvailableSpy.wait();
        QCOMPARE(downloadAvailableSpy.size(), 1);
        QVERIFY(redirectHit);

        ConfigFile cfg;
        QSettings settings(cfg.configFile(), QSettings::IniFormat);
        const auto downloadedUpdateFilePath = settings.value("Updater/updateAvailable"_L1).toString(); // anonymous const in ocupdater.cpp
        const auto expectedUpdateFilePath = FileSystem::joinPath(cfg.configPath(), "Nextcloud.msi");
        QCOMPARE(downloadedUpdateFilePath, expectedUpdateFilePath);
        QFile updateFile(expectedUpdateFilePath);
        QVERIFY(updateFile.open(QIODevice::ReadOnly));
        const auto updateContents = updateFile.readAll();
        updateFile.close();
        QCOMPARE(updateContents, "This would be the installer"_ba);
    }
#endif
};

QTEST_GUILESS_MAIN(TestUpdater)
#include "testupdater.moc"
