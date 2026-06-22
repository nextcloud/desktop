/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include <QDebug>

#include "accessmanager.h"
#include "creds/webflowcredentials.h"
#include "propagatedownload.h"
#include "owncloudpropagator_p.h"
#include "syncenginetestutils.h"

using namespace Qt::StringLiterals;

#ifdef HAVE_QHTTPSERVER
#include <QHttpServer>
#include <QTcpServer>
#endif

using namespace OCC;
namespace OCC {
QString OWNCLOUDSYNC_EXPORT createDownloadTmpFileName(const QString &previous);
}

class TestNextcloudPropagator : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testUpdateErrorFromSession()
    {
        //OwncloudPropagator propagator(nullptr, QLatin1String("test1"), QLatin1String("test2"), new ProgressDatabase);
        QVERIFY( true );
    }

    void testTmpDownloadFileNameGeneration()
    {
        QString fn;
        // without dir
        for (int i = 1; i <= 1000; i++) {
            fn+="F";
            QString tmpFileName = createDownloadTmpFileName(fn);
            if (tmpFileName.contains('/')) {
                tmpFileName = tmpFileName.mid(tmpFileName.lastIndexOf('/')+1);
            }
            QVERIFY( tmpFileName.length() > 0);
            QVERIFY( tmpFileName.length() <= 254);
        }
        // with absolute dir
        fn = "/Users/guruz/ownCloud/rocks/GPL";
        for (int i = 1; i < 1000; i++) {
            fn+="F";
            QString tmpFileName = createDownloadTmpFileName(fn);
            if (tmpFileName.contains('/')) {
                tmpFileName = tmpFileName.mid(tmpFileName.lastIndexOf('/')+1);
            }
            QVERIFY( tmpFileName.length() > 0);
            QVERIFY( tmpFileName.length() <= 254);
        }
        // with relative dir
        fn = "rocks/GPL";
        for (int i = 1; i < 1000; i++) {
            fn+="F";
            QString tmpFileName = createDownloadTmpFileName(fn);
            if (tmpFileName.contains('/')) {
                tmpFileName = tmpFileName.mid(tmpFileName.lastIndexOf('/')+1);
            }
            QVERIFY( tmpFileName.length() > 0);
            QVERIFY( tmpFileName.length() <= 254);
        }
    }

    void testParseEtag()
    {
        using Test = QPair<const char*, const char*>;
        QList<Test> tests;
        tests.append(Test("\"abcd\"", "abcd"));
        tests.append(Test("\"\"", ""));
        tests.append(Test("\"fii\"-gzip", "fii"));
        tests.append(Test("W/\"foo\"", "foo"));

        for (const auto &test : tests) {
            QCOMPARE(parseEtag(test.first), QByteArray(test.second));
        }
    }

    void testParseException()
    {
        QNetworkRequest request;
        request.setUrl(QStringLiteral("http://cloud.example.de/"));
        const auto body = QByteArrayLiteral(
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<d:error xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\">\n"
            "<s:exception>Sabre\\Exception\\UnsupportedMediaType</s:exception>\n"
            "<s:message>Virus detected!</s:message>\n"
            "</d:error>");
        const auto reply = new FakeErrorReply(QNetworkAccessManager::PutOperation,
                                              request,
                                              this,
                                              415, body);
        const auto exceptionParsed = OCC::getExceptionFromReply(reply);
        // verify parsing succeeded
        QVERIFY(!exceptionParsed.first.isEmpty());
        QVERIFY(!exceptionParsed.second.isEmpty());
        // verify buffer is not changed
        QCOMPARE(reply->readAll().size(), body.size());
    }

#ifdef HAVE_QHTTPSERVER
    void testGETFileJobDecompressionThreshold()
    {
        // generate 50 MiB of easily compressable data, and compress it with
        // the highest possible level to achieve a big enough decompression
        // ratio for Qt's decompression check to kick in
        const QByteArray decompressedContent(50 * 1024 * 1024, 'A');
        // skip the first 4 bytes of compression header to only serve the pure
        // deflate stream
        const auto compressedContent = qCompress(decompressedContent, 9).mid(4);
        // ensure the ratio is greater than 40:1 (default for gzip/deflate as
        // per Qt docs)
        const auto ratio = double(decompressedContent.size()) / (compressedContent.size());
        qInfo() << "Compression ratio is" << ratio;
        QCOMPARE_GT(ratio, 40.0);

        // spin up a temprary test-specific HTTP server that serves the
        // compressed data
        // with a fake QNAM and overrides we would skip the decompression checks
        // done internally in QNetworkReply
        QHttpServer httpServer;
        httpServer.route("/remote.php/dav/files/admin/someTestFile", [&compressedContent](QHttpServerResponder &responder) -> void {
            QHttpHeaders headers;
            headers.append(QHttpHeaders::WellKnownHeader::ContentEncoding, "deflate"_ba);
            headers.append("OC-ETag"_ba, "0123456789abcdef"_ba);
            headers.append("ETag"_ba, "0123456789abcdef"_ba);
            responder.write(compressedContent, headers);
        });

        QTcpServer tcpServer;
        QVERIFY(tcpServer.listen(QHostAddress::LocalHost));
        QVERIFY(httpServer.bind(&tcpServer));
        const QString baseUrl = "http://%1:%2"_L1.arg(tcpServer.serverAddress().toString(), QString::number(tcpServer.serverPort()));
        qInfo() << "Listening on" << baseUrl;

        auto account = OCC::Account::create();
        account->setCredentials(new FakeCredentials{new OCC::AccessManager{this}});
        account->setUrl(baseUrl);

        {
            qInfo() << "Test: with default decompression threshold (20 MiB)";
            QBuffer receivedContent;
            receivedContent.open(QIODevice::ReadWrite);

            auto job = GETFileJob(account, "/someTestFile"_L1, &receivedContent, {}, {}, 0);
            QSignalSpy spy(&job, &GETFileJob::finishedSignal);

            job.start();
            spy.wait(1000);

            QVERIFY(spy.isEmpty()); // the request failed, so the finishedSignal never was emitted
            QVERIFY(job.reply());
            QCOMPARE(job.reply()->error(), QNetworkReply::UnknownContentError);
            // the download was aborted below the configured threshold
            QCOMPARE_LT(receivedContent.size(), job.reply()->request().decompressedSafetyCheckThreshold());
        }

        {
            qInfo() << "Test with decompression threshold set from expected file size (50 MiB + default 20MiB)";
            QBuffer receivedContent;
            receivedContent.open(QIODevice::WriteOnly);

            auto job = GETFileJob(account, "/someTestFile"_L1, &receivedContent, {}, {}, 0);
            QSignalSpy spy(&job, &GETFileJob::finishedSignal);

            job.setDecompressionThresholdBase(decompressedContent.size()); // expect a response of 50MiB
            job.start();
            spy.wait(1000);

            QVERIFY(!spy.isEmpty());
            QVERIFY(job.reply());
            QCOMPARE(job.reply()->error(), QNetworkReply::NoError);
            // the download succeeded
            QCOMPARE(receivedContent.size(), decompressedContent.size());
        }
    }

    void testDirectUrlCredentials()
    {
        // for this one we need two HTTP servers:
        // - one that acts as a host for direct downloads
        // - and another one that's our pretend-Nextcloud server
        //
        // in any case the requests made to the direct download server should
        // never contain the `Authorization` header

        QHttpServer directDownloadServer;
        directDownloadServer.route("/data/directData", [](const QHttpServerRequest &request, QHttpServerResponder &responder) -> void {
            QVERIFY(!request.headers().contains(QHttpHeaders::WellKnownHeader::Authorization));

            QHttpHeaders headers;
            headers.append(QHttpHeaders::WellKnownHeader::ContentType, "text/plain"_ba);
            responder.write("OK directData"_ba, headers);
        });
        directDownloadServer.route("/data/redirectToOtherUrl", [](const QHttpServerRequest &request, QHttpServerResponder &responder) -> void {
            QVERIFY(!request.headers().contains(QHttpHeaders::WellKnownHeader::Authorization));

            QHttpHeaders headers;
            headers.append(QHttpHeaders::WellKnownHeader::Location, "/someOtherPath/redirectTarget"_ba);
            responder.write(""_ba, headers, QHttpServerResponder::StatusCode::Found);
        });
        directDownloadServer.route("/someOtherPath/redirectTarget", [](const QHttpServerRequest &request, QHttpServerResponder &responder) -> void {
            QVERIFY(!request.headers().contains(QHttpHeaders::WellKnownHeader::Authorization));

            QHttpHeaders headers;
            headers.append(QHttpHeaders::WellKnownHeader::ContentType, "text/plain"_ba);
            headers.append("OC-ETag"_ba, "0123456789abcdef"_ba);
            headers.append("ETag"_ba, "0123456789abcdef"_ba);
            responder.write("OK redirectTarget"_ba, headers);
        });

        QTcpServer directDownloadTcpServer;
        QVERIFY(directDownloadTcpServer.listen(QHostAddress::LocalHost));
        QVERIFY(directDownloadServer.bind(&directDownloadTcpServer));
        const QString directDownloadBaseUrl = "http://%1:%2"_L1.arg(directDownloadTcpServer.serverAddress().toString(), QString::number(directDownloadTcpServer.serverPort()));
        qInfo() << "Listening on" << directDownloadBaseUrl;

        QHttpServer nextcloudServer;
        nextcloudServer.route("/remote.php/dav/files/propagatortest/someFile", [](const QHttpServerRequest &request, QHttpServerResponder &responder) -> void {
            QVERIFY(request.headers().contains(QHttpHeaders::WellKnownHeader::Authorization));

            QHttpHeaders headers;
            headers.append(QHttpHeaders::WellKnownHeader::ContentType, "text/plain"_ba);
            headers.append("OC-ETag"_ba, "0123456789abcdef"_ba);
            headers.append("ETag"_ba, "0123456789abcdef"_ba);
            responder.write("OK someFile"_ba, headers);
        });
        nextcloudServer.route("/remote.php/dav/files/propagatortest/redirectedFile", [&directDownloadBaseUrl](const QHttpServerRequest &request, QHttpServerResponder &responder) -> void {
            QVERIFY(request.headers().contains(QHttpHeaders::WellKnownHeader::Authorization));

            QHttpHeaders headers;
            headers.append(QHttpHeaders::WellKnownHeader::ContentType, "text/plain"_ba);
            headers.append(QHttpHeaders::WellKnownHeader::Location, directDownloadBaseUrl + "/someOtherPath/redirectTarget"_L1);
            responder.write(""_ba, headers, QHttpServerResponder::StatusCode::Found);
        });

        QTcpServer nextcloudTcpServer;
        QVERIFY(nextcloudTcpServer.listen(QHostAddress::LocalHost));
        QVERIFY(nextcloudServer.bind(&nextcloudTcpServer));
        const QString nextcloudBaseUrl = "http://%1:%2"_L1.arg(nextcloudTcpServer.serverAddress().toString(), QString::number(nextcloudTcpServer.serverPort()));
        qInfo() << "Listening on" << nextcloudBaseUrl;


        auto account = OCC::Account::create();
        // using WebFlowCredentials here to to make sure it works as expected for real
        account->setCredentials(new WebFlowCredentials(u"propagatortest"_s, u"invalid"_s));
        account->setDavUser(u"propagatortest"_s);
        account->setUrl(nextcloudBaseUrl);


        {
            qInfo() << "Test: direct URL";
            QBuffer receivedContent;
            receivedContent.open(QIODevice::ReadWrite);

            auto job = GETFileJob(account, QUrl{directDownloadBaseUrl + "/data/directData"_L1}, &receivedContent, {}, {}, 0);
            QSignalSpy spy(&job, &GETFileJob::finishedSignal);

            job.start();
            spy.wait(1000);

            // The temporary web servers verify the received headers
            QCOMPARE(receivedContent.data(), "OK directData"_ba);
        }

        {
            qInfo() << "Test: direct URL with redirect";
            QBuffer receivedContent;
            receivedContent.open(QIODevice::ReadWrite);

            auto job = GETFileJob(account, QUrl{directDownloadBaseUrl + "/data/redirectToOtherUrl"_L1}, &receivedContent, {}, {}, 0);
            QSignalSpy spy(&job, &GETFileJob::finishedSignal);

            job.start();
            spy.wait(1000);

            // The temporary web servers verify the received headers
            QCOMPARE(receivedContent.data(), "OK redirectTarget"_ba);
        }

        {
            qInfo() << "Test: standard dav path";
            QBuffer receivedContent;
            receivedContent.open(QIODevice::ReadWrite);

            auto job = GETFileJob(account, "/someFile"_L1, &receivedContent, {}, {}, 0);
            QSignalSpy spy(&job, &GETFileJob::finishedSignal);

            job.start();
            spy.wait(1000);

            // The temporary web servers verify the received headers
            QCOMPARE(receivedContent.data(), "OK someFile"_ba);
        }

        {
            qInfo() << "Test: standard dav path with a redirect to a different origin";
            QBuffer receivedContent;
            receivedContent.open(QIODevice::ReadWrite);

            auto job = GETFileJob(account, "/redirectedFile"_L1, &receivedContent, {}, {}, 0);
            QSignalSpy spy(&job, &GETFileJob::finishedSignal);

            job.start();
            spy.wait(1000);

            // The temporary web servers verify the received headers
            QCOMPARE(receivedContent.data(), "OK redirectTarget"_ba);
        }
    }
#endif
};

QTEST_GUILESS_MAIN(TestNextcloudPropagator)
#include "testnextcloudpropagator.moc"
