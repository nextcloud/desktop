/*
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 *
 */

#include <QtTest>
#include <QDir>
#include <QString>

#include "checksums.h"
#include "networkjobs.h"
#include "utility.h"
#include "filesystem.h"
#include "propagatorjobs.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
// poor man QTRY_VERIFY when Qt5 is not available.
#define QTRY_VERIFY(Cond) QTest::qWait(1000); QVERIFY(Cond)
#endif

using namespace OCC;

    class TestChecksumValidator : public QObject
    {
        Q_OBJECT

    private:
        QString _root;
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

    private slots:

    void initTestCase() {
        _root = QDir::tempPath() + "/" + "test_" + QString::number(qrand());
        QDir rootDir(_root);

        rootDir.mkpath(_root );
        _testfile = _root+"/csFile";
        Utility::writeRandomFile( _testfile);
    }

    void testUploadChecksummingAdler() {
#ifndef ZLIB_FOUND
        QSKIP("ZLIB not found.", SkipSingle);
#else
        ComputeChecksum *vali = new ComputeChecksum(this);
        _expectedType = "Adler32";
        vali->setChecksumType(_expectedType);

        connect(vali, SIGNAL(done(QByteArray,QByteArray)), SLOT(slotUpValidated(QByteArray,QByteArray)));

        _expected = FileSystem::calcAdler32( _testfile );
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

        _expected = FileSystem::calcMd5( _testfile );
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

        _expected = FileSystem::calcSha1( _testfile );

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
        adler.append(FileSystem::calcAdler32( _testfile ));
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

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
// Qt4 does not have QTEST_GUILESS_MAIN, so we simulate it.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    TestChecksumValidator tc;
    return QTest::qExec(&tc, argc, argv);
}
#else
    QTEST_GUILESS_MAIN(TestChecksumValidator)
#endif

#include "testchecksumvalidator.moc"
