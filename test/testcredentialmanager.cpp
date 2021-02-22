/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */
#include "account.h"
#include "libsync/creds/credentialmanager.h"

#include "syncenginetestutils.h"

#include <QTest>

namespace OCC {

class TestCredentialManager : public QObject
{
    Q_OBJECT

    bool _finished = false;

    QTemporaryFile _credStoreFile;
    QSettings _credStore;

    void setFallbackEnabled(QKeychain::Job *job)
    {
        // store the test credentials in a plain text settings file on unsupported platforms
        job->setSettings(&_credStore);
        job->setInsecureFallback(true);
    }

private Q_SLOTS:
    void init()
    {
        _finished = false;
        QVERIFY(_credStoreFile.open());
        _credStore.setPath(QSettings::IniFormat, QSettings::UserScope, _credStoreFile.fileName());
        _credStore.clear();
    }

    void testSetGet_data()
    {
        QTest::addColumn<QVariant>("data");

        QTest::newRow("bool") << QVariant::fromValue(true);
        QTest::newRow("int") << QVariant::fromValue(1);
        QTest::newRow("map") << QVariant::fromValue(QVariantMap { { "foo", QColor(Qt::red) }, { "bar", "42" } });
    }

    void testSetGet()
    {
        QFETCH(QVariant, data);
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };
        auto creds = fakeFolder.account()->credentialManager();

        const QString key = QStringLiteral("test");
        auto job = creds->set(key, data);
        setFallbackEnabled(job);

        connect(job, &QKeychain::Job::finished, this, [creds, data, key, this] {
            auto job = creds->get(key);
            setFallbackEnabled(job->_job);
            connect(job, &CredentialJob::finished, this, [job, data, creds, this] {
                QCOMPARE(job->data(), data);
                const auto jobs = creds->clear();
                for (auto &job : jobs) {
                    setFallbackEnabled(job);
                }
                connect(jobs[0], &QKeychain::Job::finished, this, [creds, data, this] {
                    QVERIFY(creds->knownKeys().isEmpty());
                    _finished = true;
                });
            });
        });
        QTest::qWaitFor([this] { return _finished; });
    }
};
}

QTEST_GUILESS_MAIN(OCC::TestCredentialManager)
#include "testcredentialmanager.moc"
