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
#include "networkjobs.h"
#include "common/utility.h"
#include "filesystem.h"
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
        QByteArray     _expected;
        QByteArray     _expectedType;
        bool           _successDown;
        bool           _errorSeen;

    public slots:

    void slotUpValidated(const QByteArray& type, const QByteArray& checksum) {
         qDebug() << "Checksum: " << checksum;
         QVERIFY(_expected == checksum );
         QVERIFY(_expectedType == type );
    }

    void slotDownValidated() {
         _successDown = true;
    }

    void slotDownError( const QString& errMsg ) {
         QVERIFY(_expectedError == errMsg );
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

    void initTestCase() {
        _testfile = _root.path()+"/csFile";
        Utility::writeRandomFile( _testfile);
    }

    void testMd5Calc()
    {
        QString file( _root.path() + "/file_a.bin");
        QVERIFY(writeRandomFile(file));
        QFileInfo fi(file);
        QVERIFY(fi.exists());
        QByteArray sum = calcMd5(file);

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
        QByteArray sum = calcSha1(file);

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
        ComputeChecksum *vali = new ComputeChecksum(this);
        _expectedType = "Adler32";
        vali->setChecksumType(_expectedType);

        connect(vali, SIGNAL(done(QByteArray,QByteArray)), SLOT(slotUpValidated(QByteArray,QByteArray)));

        _expected = calcAdler32( _testfile );
        qDebug() << "XX Expected Checksum: " << _expected;
        vali->start(_testfile);

        QEventLoop loop;
        connect(vali, SIGNAL(done(QByteArray,QByteArray)), &loop, SLOT(quit()), Qt::QueuedConnection);
        loop.exec();

        delete vali;
#endif
    }

    void testUploadChecksummingMd5() {

        ComputeChecksum *vali = new ComputeChecksum(this);
        _expectedType = OCC::checkSumMD5C;
        vali->setChecksumType(_expectedType);
        connect(vali, SIGNAL(done(QByteArray,QByteArray)), this, SLOT(slotUpValidated(QByteArray,QByteArray)));

        _expected = calcMd5( _testfile );
        vali->start(_testfile);

        QEventLoop loop;
        connect(vali, SIGNAL(done(QByteArray,QByteArray)), &loop, SLOT(quit()), Qt::QueuedConnection);
        loop.exec();

        delete vali;
    }

    void testUploadChecksummingSha1() {

        ComputeChecksum *vali = new ComputeChecksum(this);
        _expectedType = OCC::checkSumSHA1C;
        vali->setChecksumType(_expectedType);
        connect(vali, SIGNAL(done(QByteArray,QByteArray)), this, SLOT(slotUpValidated(QByteArray,QByteArray)));

        _expected = calcSha1( _testfile );

        vali->start(_testfile);

        QEventLoop loop;
        connect(vali, SIGNAL(done(QByteArray,QByteArray)), &loop, SLOT(quit()), Qt::QueuedConnection);
        loop.exec();

        delete vali;
    }

    void testDownloadChecksummingAdler() {
#ifndef ZLIB_FOUND
        QSKIP("ZLIB not found.", SkipSingle);
#else
        QByteArray adler =  checkSumAdlerC;
        adler.append(":");
        adler.append(calcAdler32( _testfile ));
        _successDown = false;

        ValidateChecksumHeader *vali = new ValidateChecksumHeader(this);
        connect(vali, SIGNAL(validated(QByteArray,QByteArray)), this, SLOT(slotDownValidated()));
        connect(vali, SIGNAL(validationFailed(QString)), this, SLOT(slotDownError(QString)));
        vali->start(_testfile, adler);

        QTRY_VERIFY(_successDown);

        _expectedError = QLatin1String("The downloaded file does not match the checksum, it will be resumed.");
        _errorSeen = false;
        vali->start(_testfile, "Adler32:543345");
        QTRY_VERIFY(_errorSeen);

        _expectedError = QLatin1String("The checksum header contained an unknown checksum type 'Klaas32'");
        _errorSeen = false;
        vali->start(_testfile, "Klaas32:543345");
        QTRY_VERIFY(_errorSeen);

        delete vali;
#endif
    }


    void cleanupTestCase() {
    }
};

    QTEST_GUILESS_MAIN(TestChecksumValidator)

#include "testchecksumvalidator.moc"
