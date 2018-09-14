/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>
#include <localdiscoverytracker.h>

using namespace OCC;

struct FakeBrokenXmlPropfindReply : FakePropfindReply {
    FakeBrokenXmlPropfindReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op,
                               const QNetworkRequest &request, QObject *parent)
        : FakePropfindReply(remoteRootFileInfo, op, request, parent) {
        QVERIFY(payload.size() > 50);
        // turncate the XML
        payload.chop(20);
    }
};

struct MissingPermissionsPropfindReply : FakePropfindReply {
    MissingPermissionsPropfindReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op,
                               const QNetworkRequest &request, QObject *parent)
        : FakePropfindReply(remoteRootFileInfo, op, request, parent) {
        // If the propfind contains a single file without permissions, this is a server error
        const char toRemove[] = "<oc:permissions>RDNVCKW</oc:permissions>";
        auto pos = payload.indexOf(toRemove, payload.size()/2);
        QVERIFY(pos > 0);
        payload.remove(pos, sizeof(toRemove) - 1);
    }
};

enum ErrorKind : int {
    // Lower code are corresponding to HTML error code
    InvalidXML = 1000,
    MissingPermissions,
    Timeout,
};

Q_DECLARE_METATYPE(ErrorCategory)

class TestRemoteDiscovery : public QObject
{
    Q_OBJECT

private slots:

    void testRemoteDiscoveryError_data()
    {
        qRegisterMetaType<ErrorCategory>();
        QTest::addColumn<int>("errorKind");
        QTest::addColumn<QString>("expectedErrorString");

        QTest::newRow("404") << 404 << "B"; // The filename should be in the error message
        QTest::newRow("500") << 500 << "Internal Server Fake Error"; // the message from FakeErrorReply
        QTest::newRow("503") << 503 << "";
        QTest::newRow("200") << 200 << ""; // 200 should be an error since propfind should return 207
        QTest::newRow("InvalidXML") << +InvalidXML << "";
        QTest::newRow("MissingPermissions") << +MissingPermissions << "missing data";
        QTest::newRow("Timeout") << +Timeout << "";
    }


    // Check what happens when there is an error.
    void testRemoteDiscoveryError()
    {
        QFETCH(int, errorKind);
        QFETCH(QString, expectedErrorString);
        bool syncSucceeds = errorKind == 503; // 503 just ignore the temporarily unavailable directory

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        // Do Some change as well
        fakeFolder.localModifier().insert("A/z1");
        fakeFolder.localModifier().insert("B/z1");
        fakeFolder.localModifier().insert("C/z1");
        fakeFolder.remoteModifier().insert("A/z2");
        fakeFolder.remoteModifier().insert("B/z2");
        fakeFolder.remoteModifier().insert("C/z2");

        auto oldLocalState = fakeFolder.currentLocalState();
        auto oldRemoteState = fakeFolder.currentRemoteState();

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *)
                -> QNetworkReply *{
            if (req.attribute(QNetworkRequest::CustomVerbAttribute) == "PROPFIND" && req.url().path().endsWith("/B")) {
                if (errorKind == InvalidXML) {
                    return new FakeBrokenXmlPropfindReply(fakeFolder.remoteModifier(), op, req, this);
                } else if (errorKind == MissingPermissions) {
                    return new MissingPermissionsPropfindReply(fakeFolder.remoteModifier(), op, req, this);
                } else if (errorKind == Timeout) {
                    return new FakeHangingReply(op, req, this);
                } else if (errorKind < 1000) {
                    return new FakeErrorReply(op, req, this, errorKind);
                }
            }
            return nullptr;
        });

        // So the test that test timeout finishes fast
        QScopedValueRollback<int> setHttpTimeout(AbstractNetworkJob::httpTimeout, errorKind == Timeout ? 1 : 10000);

        QSignalSpy errorSpy(&fakeFolder.syncEngine(), &SyncEngine::syncError);
        QCOMPARE(fakeFolder.syncOnce(), false);
        qDebug() << "errorSpy=" << errorSpy;

        // The folder B should not have been sync'ed (and in particular not removed)
        QCOMPARE(oldLocalState.children["B"], fakeFolder.currentLocalState().children["B"]);
        QCOMPARE(oldRemoteState.children["B"], fakeFolder.currentRemoteState().children["B"]);
        if (!syncSucceeds) {
            // Check we got the right error
            QCOMPARE(errorSpy.count(), 1);
            QVERIFY(errorSpy[0][0].toString().contains(expectedErrorString));
        } else {
            // The other folder should have been sync'ed as the sync just ignored the faulty dir
            QCOMPARE(fakeFolder.currentRemoteState().children["A"], fakeFolder.currentLocalState().children["A"]);
            QCOMPARE(fakeFolder.currentRemoteState().children["C"], fakeFolder.currentLocalState().children["C"]);
        }
    }
};

QTEST_GUILESS_MAIN(TestRemoteDiscovery)
#include "testremotediscovery.moc"
