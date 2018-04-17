/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;

static constexpr quint64 stopAfter = 3'123'668;

/* A FakeGetReply that sends max 'fakeSize' bytes, but whose ContentLength has the corect size */
class BrokenFakeGetReply : public FakeGetReply
{
    Q_OBJECT
public:
    using FakeGetReply::FakeGetReply;
    int fakeSize = stopAfter;

    qint64 bytesAvailable() const override
    {
        if (aborted)
            return 0;
        return std::min(size, fakeSize) + QIODevice::bytesAvailable();
    }

    qint64 readData(char *data, qint64 maxlen) override
    {
        qint64 len = std::min(qint64{ fakeSize }, maxlen);
        std::fill_n(data, len, payload);
        size -= len;
        fakeSize -= len;
        return len;
    }
};


SyncFileItemPtr getItem(const QSignalSpy &spy, const QString &path)
{
    for (const QList<QVariant> &args : spy) {
        auto item = args[0].value<SyncFileItemPtr>();
        if (item->destination() == path)
            return item;
    }
    return {};
}


class TestDownload : public QObject
{
    Q_OBJECT

private slots:

    void testResume()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
        auto size = 30 * 1000 * 1000;
        fakeFolder.remoteModifier().insert("A/a0", size);

        // First, download only the first 3 MB of the file
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith("A/a0")) {
                return new BrokenFakeGetReply(fakeFolder.remoteModifier(), op, request, this);
            }
            return nullptr;
        });

        QVERIFY(!fakeFolder.syncOnce()); // The sync must fail because not all the file was downloaded
        QCOMPARE(getItem(completeSpy, "A/a0")->_status, SyncFileItem::SoftError);
        QCOMPARE(getItem(completeSpy, "A/a0")->_errorString, QString("The file could not be downloaded completely."));
        QVERIFY(fakeFolder.syncEngine().isAnotherSyncNeeded());

        // Now, we need to restart, this time, it should resume.
        QByteArray ranges;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith("A/a0")) {
                ranges = request.rawHeader("Range");
            }
            return nullptr;
        });
        QVERIFY(fakeFolder.syncOnce()); // now this succeeds
        QCOMPARE(ranges, QByteArray("bytes=" + QByteArray::number(stopAfter) + "-"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
};

QTEST_GUILESS_MAIN(TestDownload)
#include "testdownload.moc"
