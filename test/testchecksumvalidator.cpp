/*
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 *
 */

#include <QtTest>
#include <QDir>
#include <QString>

#include "common/checksums.h"
#include "common/utility.h"
#include "filesystem.h"
#include "networkjobs.h"
#include "propagatorjobs.h"
#include "testutils/testutils.h"

using namespace OCC;
using namespace OCC::Utility;

    class TestChecksumValidator : public QObject
    {
        Q_OBJECT
    private:
        const QTemporaryDir _root = TestUtils::createTempDir();
        QString _testfile;
        QString _expectedError;
        QByteArray     _expected;
        CheckSums::Algorithm _expectedType;
        bool           _successDown;
        bool           _errorSeen;

    public slots:

        void slotUpValidated(CheckSums::Algorithm type, const QByteArray &checksum)
        {
            qDebug() << "Checksum: " << checksum;
            QCOMPARE(_expected, checksum);
            QVERIFY(_expectedType == type);
        }

    void slotDownValidated() {
         _successDown = true;
    }

    void slotDownError( const QString& errMsg ) {
         QCOMPARE(_expectedError, errMsg);
         _errorSeen = true;
    }

    static QByteArray shellSum( const QByteArray& cmd, const QString& file )
    {
        QProcess md5;
        QStringList args;
        args.append(file);
        md5.start(QString::fromUtf8(cmd), args);
        QByteArray sumShell;
        qDebug() << "File: "<< file;

        if( md5.waitForFinished()  ) {

            sumShell = md5.readAll();
            sumShell = sumShell.left( sumShell.indexOf(' '));
        }
        return sumShell;
    }

    private slots:

    void initTestCase() {
        _testfile = _root.path() + QStringLiteral("/csFile");
        TestUtils::writeRandomFile(_testfile);
    }

    void testMd5Calc()
    {
        QString file(_root.path() + QStringLiteral("/file_a.bin"));
        QVERIFY(TestUtils::writeRandomFile(file));
        QFileInfo fi(file);
        QVERIFY(fi.exists());

        QFile fileDevice(file);
        fileDevice.open(QIODevice::ReadOnly);
        QByteArray sum = ComputeChecksum::computeNow(&fileDevice, CheckSums::Algorithm::MD5);
        fileDevice.close();

        QByteArray sSum = shellSum("md5sum", file);
        if (sSum.isEmpty())
            QSKIP("Couldn't execute md5sum to calculate checksum, executable missing?", SkipSingle);

        QVERIFY(!sum.isEmpty());
        QCOMPARE(sSum, sum);
    }

    void testSha1Calc()
    {
        QString file(_root.path() + QStringLiteral("/file_b.bin"));
        TestUtils::writeRandomFile(file);
        QFileInfo fi(file);
        QVERIFY(fi.exists());

        QFile fileDevice(file);
        fileDevice.open(QIODevice::ReadOnly);
        QByteArray sum = ComputeChecksum::computeNow(&fileDevice, CheckSums::Algorithm::SHA1);
        fileDevice.close();

        QByteArray sSum = shellSum("sha1sum", file);
        if (sSum.isEmpty())
            QSKIP("Couldn't execute sha1sum to calculate checksum, executable missing?", SkipSingle);

        QVERIFY(!sum.isEmpty());
        QCOMPARE(sSum, sum);
    }

    void testUploadChecksummingAdler() {
        ComputeChecksum *vali = new ComputeChecksum(this);
        _expectedType = CheckSums::Algorithm::ADLER32;
        vali->setChecksumType(_expectedType);

        connect(vali, &ComputeChecksum::done, this, &TestChecksumValidator::slotUpValidated);

        auto file = QFile(_testfile);
        file.open(QIODevice::ReadOnly);
        _expected = ComputeChecksum::computeNow(&file, CheckSums::Algorithm::ADLER32);
        qDebug() << "XX Expected Checksum: " << _expected;
        vali->start(_testfile);

        QEventLoop loop;
        connect(vali, &ComputeChecksum::done, &loop, &QEventLoop::quit, Qt::QueuedConnection);
        loop.exec();

        delete vali;
    }

    void testUploadChecksummingMd5() {

        ComputeChecksum *vali = new ComputeChecksum(this);
        _expectedType = OCC::CheckSums::Algorithm::MD5;
        vali->setChecksumType(_expectedType);
        connect(vali, &ComputeChecksum::done, this, &TestChecksumValidator::slotUpValidated);

        auto file = QFile(_testfile);
        file.open(QIODevice::ReadOnly);
        _expected = ComputeChecksum::computeNow(&file, CheckSums::Algorithm::MD5);
        vali->start(_testfile);

        QEventLoop loop;
        connect(vali, &ComputeChecksum::done, &loop, &QEventLoop::quit, Qt::QueuedConnection);
        loop.exec();

        delete vali;
    }

    void testUploadChecksummingSha1() {
        ComputeChecksum *vali = new ComputeChecksum(this);
        _expectedType = OCC::CheckSums::Algorithm::SHA1;
        vali->setChecksumType(_expectedType);
        connect(vali, &ComputeChecksum::done, this, &TestChecksumValidator::slotUpValidated);

        auto file = QFile(_testfile);
        file.open(QIODevice::ReadOnly);
        _expected = ComputeChecksum::computeNow(&file, CheckSums::Algorithm::SHA1);

        vali->start(_testfile);

        QEventLoop loop;
        connect(vali, &ComputeChecksum::done, &loop, &QEventLoop::quit, Qt::QueuedConnection);
        loop.exec();

        delete vali;
    }

    void testDownloadChecksummingAdler() {
        ValidateChecksumHeader *vali = new ValidateChecksumHeader(this);
        connect(vali, &ValidateChecksumHeader::validated, this, &TestChecksumValidator::slotDownValidated);
        connect(vali, &ValidateChecksumHeader::validationFailed, this, &TestChecksumValidator::slotDownError);

        auto file = QFile(_testfile);
        file.open(QIODevice::ReadOnly);
        _expected = ComputeChecksum::computeNow(&file, CheckSums::Algorithm::ADLER32);

        QByteArray adler = OCC::CheckSums::toString(OCC::CheckSums::Algorithm::ADLER32).data();
        adler.append(":");
        adler.append(_expected);

        file.seek(0);
        _successDown = false;
        vali->start(_testfile, adler);

        QTRY_VERIFY(_successDown);

        _expectedError = QStringLiteral("The downloaded file does not match the checksum, it will be resumed. '543345' != '%1'").arg(QString::fromUtf8(_expected));
        _errorSeen = false;
        file.seek(0);
        vali->start(_testfile, "Adler32:543345");
        QTRY_VERIFY(_errorSeen);

        _expectedError = QStringLiteral("The checksum header contained an unknown checksum type 'Klaas32'");
        _errorSeen = false;
        file.seek(0);
        vali->start(_testfile, "Klaas32:543345");
        QTRY_VERIFY(_errorSeen);

        delete vali;
    }


    void cleanupTestCase() {
    }
};

    QTEST_GUILESS_MAIN(TestChecksumValidator)

#include "testchecksumvalidator.moc"
