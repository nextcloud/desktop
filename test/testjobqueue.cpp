/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */
#include "jobqueue.h"

#include "abstractnetworkjob.h"
#include "account.h"

#include "testutils/syncenginetestutils.h"

#include <QTest>

using namespace OCC;

class TestJob : public AbstractNetworkJob
{
    // AbstractNetworkJob interface
public:
    // TODO: davurl
    TestJob(AccountPtr account)
        : AbstractNetworkJob(account, account->davUrl(), QStringLiteral("/A/a1"))
    {
    }

    void start() override
    {
        QNetworkRequest req;
        req.setRawHeader("Depth", "0");

        QByteArray xml("<?xml version=\"1.0\" ?>\n"
                       "<d:propfind xmlns:d=\"DAV:\">\n"
                       "  <d:prop>\n"
                       "    <d:getetag/>\n"
                       "  </d:prop>\n"
                       "</d:propfind>\n");
        QBuffer *buf = new QBuffer(this);
        buf->setData(xml);
        buf->open(QIODevice::ReadOnly);
        // assumes ownership
        sendRequest("PROPFIND", req, buf);
        AbstractNetworkJob::start();
    }

protected:
    void finished() override
    {
    }
};

class TestJobQueue : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testQueue()
    {
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };

        auto queue = fakeFolder.account()->jobQueue();
        JobQueueGuard queueGuard(queue);
        QVERIFY(!queue->isBlocked());

        TestJob *job = new TestJob(fakeFolder.account());
        QVERIFY(!queue->enqueue(job));
        QCOMPARE(queue->size(), 0);
        QVERIFY(queueGuard.block());
        QVERIFY(queue->isBlocked());
        job->start();
        QCOMPARE(queue->size(), 1);
        QVERIFY(queueGuard.unblock());
        QVERIFY(!queue->isBlocked());
        QCOMPARE(queue->size(), 0);
        QCOMPARE(job->retryCount(), 1);
        QVERIFY(!queueGuard.unblock());
        QVERIFY(!queue->isBlocked());
    }

    void testMultiBlock()
    {
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };

        auto queue = fakeFolder.account()->jobQueue();
        JobQueueGuard queueGuard(queue);

        TestJob *job = new TestJob(fakeFolder.account());
        QVERIFY(!queue->enqueue(job));
        QCOMPARE(queue->size(), 0);
        QVERIFY(queueGuard.block());
        QVERIFY(!queueGuard.block());
        job->start();
        QCOMPARE(queue->size(), 1);
        QVERIFY(queueGuard.unblock());
        QCOMPARE(queue->size(), 0);
        QVERIFY(!queueGuard.unblock());
        QCOMPARE(job->retryCount(), 1);
    }

    void testMultiBlock2()
    {
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };

        auto queue = fakeFolder.account()->jobQueue();
        JobQueueGuard queueGuard1(queue);

        TestJob *job = new TestJob(fakeFolder.account());
        {
            JobQueueGuard queueGuard2(queue);
            QVERIFY(!queue->enqueue(job));
            QCOMPARE(queue->size(), 0);
            QVERIFY(queueGuard1.block());
            QVERIFY(queueGuard2.block());
            job->start();
            QVERIFY(queue->isBlocked());
            QCOMPARE(queue->size(), 1);
            QVERIFY(queueGuard1.unblock());
            QVERIFY(queue->isBlocked());
            QCOMPARE(queue->size(), 1);
            QVERIFY(queueGuard2.unblock());
            QVERIFY(!queue->isBlocked());
            QCOMPARE(queue->size(), 0);
            QCOMPARE(job->retryCount(), 1);

            QVERIFY(queueGuard2.block());
            QVERIFY(queue->isBlocked());
        }
        QVERIFY(!queue->isBlocked());
    }
};

QTEST_GUILESS_MAIN(TestJobQueue)
#include "testjobqueue.moc"
