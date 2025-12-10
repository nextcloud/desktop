/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include <QDir>
#include <QString>

#include "common/checksums.h"
#include "networkjobs.h"
#include "common/checksumcalculator.h"
#include "common/checksumconsts.h"
#include "common/utility.h"
#include "filesystem.h"
#include "logger.h"
#include "propagatorjobs.h"

using namespace OCC;
using namespace OCC::Utility;

    class TestChecksumValidator : public QObject
    {
        Q_OBJECT
    private:
        QTemporaryDir _root;
        QString _testfile;
        QString _expectedError;
        ValidateChecksumHeader::FailureReason _expectedFailureReason = ValidateChecksumHeader::FailureReason::Success;
        QByteArray     _expected;
        QByteArray     _expectedType;
        bool           _successDown = false;
        bool           _errorSeen = false;

    public slots:

    void slotUpValidated(const QByteArray& type, const QByteArray& checksum) {
         qDebug() << "Checksum: " << checksum;
         QVERIFY(_expected == checksum );
         QVERIFY(_expectedType == type );
    }

    void slotDownValidated() {
         _successDown = true;
    }

    void slotDownError(const QString &errMsg, const QByteArray &calculatedChecksumType,
        const QByteArray &calculatedChecksum, const OCC::ValidateChecksumHeader::FailureReason reason)
    {
        Q_UNUSED(calculatedChecksumType);
        Q_UNUSED(calculatedChecksum);
        QCOMPARE(_expectedError, errMsg);
        QCOMPARE(_expectedFailureReason, reason);
        _errorSeen = true;
    }

    static QByteArray shellSum( const QByteArray& cmd, const QString& file )
    {
        QProcess md5;
        QStringList args;
        args.append(file);
        md5.start(cmd, args);
        QByteArray sumShell;
        qDebug() << "File: "<< file;

        if( md5.waitForFinished()  ) {

            sumShell = md5.readAll();
            sumShell = sumShell.left( sumShell.indexOf(' '));
        }
        return sumShell;
    }

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        _testfile = _root.path()+"/csFile";
        Utility::writeRandomFile( _testfile);
    }

    void testMd5Calc()
    {
        QString file( _root.path() + "/file_a.bin");
        QVERIFY(writeRandomFile(file));
        QFileInfo fi(file);
        QVERIFY(fi.exists());

        ChecksumCalculator checksumCalculator(file, OCC::checkSumMD5C);

        const auto sum = checksumCalculator.calculate();

        QByteArray sSum = shellSum("md5sum", file);
        if (sSum.isEmpty())
            QSKIP("Couldn't execute md5sum to calculate checksum, executable missing?", SkipSingle);

        QVERIFY(!sum.isEmpty());
        QCOMPARE(sSum, sum);
    }

    void testSha1Calc()
    {
        QString file( _root.path() + "/file_b.bin");
        writeRandomFile(file);
        QFileInfo fi(file);
        QVERIFY(fi.exists());

        ChecksumCalculator checksumCalculator(file, OCC::checkSumSHA1C);

        const auto sum = checksumCalculator.calculate();

        QByteArray sSum = shellSum("sha1sum", file);
        if (sSum.isEmpty())
            QSKIP("Couldn't execute sha1sum to calculate checksum, executable missing?", SkipSingle);

        QVERIFY(!sum.isEmpty());
        QCOMPARE(sSum, sum);
    }

    void testUploadChecksummingAdler() {
#ifndef ZLIB_FOUND
        QSKIP("ZLIB not found.", SkipSingle);
#else
        auto *vali = new ComputeChecksum(this);
        _expectedType = "Adler32";
        vali->setChecksumType(_expectedType);

        connect(vali, &ComputeChecksum::done, this, &TestChecksumValidator::slotUpValidated);

        ChecksumCalculator checksumCalculator(_testfile, OCC::checkSumAdlerC);

        _expected = checksumCalculator.calculate();

        qDebug() << "XX Expected Checksum: " << _expected;
        vali->start(_testfile);

        QEventLoop loop;
        connect(vali, &ComputeChecksum::done, &loop, &QEventLoop::quit, Qt::QueuedConnection);
        loop.exec();

        delete vali;
#endif
    }

    void testUploadChecksummingMd5() {

        auto *vali = new ComputeChecksum(this);
        _expectedType = OCC::checkSumMD5C;
        vali->setChecksumType(_expectedType);
        connect(vali, &ComputeChecksum::done, this, &TestChecksumValidator::slotUpValidated);

        ChecksumCalculator checksumCalculator(_testfile, OCC::checkSumMD5C);

        _expected = checksumCalculator.calculate();
        vali->start(_testfile);

        QEventLoop loop;
        connect(vali, &ComputeChecksum::done, &loop, &QEventLoop::quit, Qt::QueuedConnection);
        loop.exec();

        delete vali;
    }

    void testUploadChecksummingSha1() {

        auto *vali = new ComputeChecksum(this);
        _expectedType = OCC::checkSumSHA1C;
        vali->setChecksumType(_expectedType);
        connect(vali, &ComputeChecksum::done, this, &TestChecksumValidator::slotUpValidated);

        ChecksumCalculator checksumCalculator(_testfile, OCC::checkSumSHA1C);
        _expected = checksumCalculator.calculate();

        vali->start(_testfile);

        QEventLoop loop;
        connect(vali, &ComputeChecksum::done, &loop, &QEventLoop::quit, Qt::QueuedConnection);
        loop.exec();

        delete vali;
    }

    void testDownloadChecksummingAdler() {
#ifndef ZLIB_FOUND
        QSKIP("ZLIB not found.", SkipSingle);
#else
        auto *vali = new ValidateChecksumHeader(this);
        connect(vali, &ValidateChecksumHeader::validated, this, &TestChecksumValidator::slotDownValidated);
        connect(vali, &ValidateChecksumHeader::validationFailed, this, &TestChecksumValidator::slotDownError);

        ChecksumCalculator checksumCalculator(_testfile, OCC::checkSumAdlerC);
        _expected = checksumCalculator.calculate();

        QByteArray adler = checkSumAdlerC;
        adler.append(":");
        adler.append(_expected);

        _successDown = false;
        vali->start(_testfile, adler);

        QTRY_VERIFY(_successDown);

        _expectedError = QStringLiteral("The downloaded file does not match the checksum, it will be resumed. \"543345\" != \"%1\"").arg(QString::fromUtf8(_expected));
        _expectedFailureReason = ValidateChecksumHeader::FailureReason::ChecksumMismatch;
        _errorSeen = false;
        vali->start(_testfile, "Adler32:543345");
        QTRY_VERIFY(_errorSeen);

        _expectedError = QLatin1String("The checksum header contained an unknown checksum type \"Klaas32\"");
        _expectedFailureReason = ValidateChecksumHeader::FailureReason::ChecksumTypeUnknown;
        _errorSeen = false;
        vali->start(_testfile, "Klaas32:543345");
        QTRY_VERIFY(_errorSeen);

        delete vali;
#endif
    }

#ifdef Q_OS_LINUX
    void testDeletingComputeChecksumBeforeCalculationCompletionDoesNotCrash()
    {
        // Running this multiple times in a loop makes it easier to run into a crash
        for (auto i = 0; i < 4096; i++) {
            auto computeChecksum = new ComputeChecksum();
            computeChecksum->setChecksumType("MD5");
            computeChecksum->start("/dev/zero");
            delete computeChecksum;
        }

        QVERIFY(true); // no crash occurred
    }
#endif

    void cleanupTestCase() {
    }
};

    QTEST_GUILESS_MAIN(TestChecksumValidator)

#include "testchecksumvalidator.moc"
