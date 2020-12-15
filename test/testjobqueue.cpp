/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */
#include "jobqueue.h"

#include "abstractnetworkjob.h"
#include "account.h"

#include "syncenginetestutils.h"

#include <QTest>

using namespace OCC;

class TestJob : public AbstractNetworkJob
{
    // AbstractNetworkJob interface
public:
    TestJob(AccountPtr account)
        : AbstractNetworkJob(account, QStringLiteral("A/a1"))
    {
    }

    void start()
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
        sendRequest("PROPFIND", makeDavUrl(path()), req, buf);
        AbstractNetworkJob::start();
    }

protected:
    bool finished()
    {
        return true;
    }
};

class TestJobQueue : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testQueue()
    {
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };

        JobQueue *queue = fakeFolder.account()->jobQueue();
        QVERIFY(!queue->isBlocked());

        TestJob *job = new TestJob(fakeFolder.account());
        QVERIFY(!queue->enqueue(job));
        QCOMPARE(queue->size(), 0);
        queue->setBlocked(true);
        QVERIFY(queue->isBlocked());
        job->start();
        QCOMPARE(queue->size(), 1);
        queue->setBlocked(false);
        QVERIFY(!queue->isBlocked());
        QCOMPARE(queue->size(), 0);
        QCOMPARE(job->retryCount(), 1);
        queue->setBlocked(false);
        QVERIFY(!queue->isBlocked());
    }

    void testMultiBlock()
    {
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };

        JobQueue *queue = fakeFolder.account()->jobQueue();

        TestJob *job = new TestJob(fakeFolder.account());
        QVERIFY(!queue->enqueue(job));
        QCOMPARE(queue->size(), 0);
        queue->setBlocked(true);
        queue->setBlocked(true);
        job->start();
        QCOMPARE(queue->size(), 1);
        queue->setBlocked(false);
        QCOMPARE(queue->size(), 1);
        queue->setBlocked(false);
        QCOMPARE(queue->size(), 0);
        QCOMPARE(job->retryCount(), 1);
    }
};

QTEST_GUILESS_MAIN(TestJobQueue)
#include "testjobqueue.moc"
