/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>

#include "account.h"
#include "accountmanager.h"
#include "logger.h"

using namespace OCC;

class TestCookieJarMigration : public QObject
{
    Q_OBJECT
    AccountPtr _account;
    QString oldCookieJarPath;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
        // Create directories used in test, since Qt doesn't create its automatically
        QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::StateLocation));
        QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));

        _account = Account::create();
        AccountManager::instance()->addAccount(_account);
        oldCookieJarPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/cookies" + _account->id() + ".db";
    }
    void testNoAction()
    {
        QFile jarFile(_account->cookieJarPath());
        jarFile.open(QFile::WriteOnly);
        jarFile.write("1", 1); // Write one byte to file
        jarFile.close();
        _account->tryMigrateCookieJar();

        QVERIFY(!QFile::exists(oldCookieJarPath)); // Check that old file doesn't exits
        QCOMPARE(QFileInfo(_account->cookieJarPath()).size(), 1); // Check that this byte present in new file
        QFile::remove(_account->cookieJarPath()); // Cleanup
    }
    void testSimpleMigration()
    {
        QFile oldJarFile(oldCookieJarPath);
        oldJarFile.open(QFile::WriteOnly);
        oldJarFile.write("1", 1); // Write one byte to file
        oldJarFile.close();

        _account->tryMigrateCookieJar();
        QVERIFY(!QFile::exists(oldCookieJarPath)); // Check that old file is deleted

        QCOMPARE(QFileInfo(_account->cookieJarPath()).size(), 1); // Check that this byte present in new file
        QFile::remove(_account->cookieJarPath()); // Cleanup
    }
    void testNotOverwrite()
    {
        QFile oldJarFile(oldCookieJarPath);
        oldJarFile.open(QFile::WriteOnly);
        oldJarFile.write("1", 1); // Write one byte to file
        oldJarFile.close();

        QFile newJarFile(_account->cookieJarPath());
        oldJarFile.open(QFile::WriteOnly);
        oldJarFile.write("123", 3); // Write three bytes to file
        oldJarFile.close();


        _account->tryMigrateCookieJar();

        QCOMPARE(QFileInfo(_account->cookieJarPath()).size(), 3); // Check that these bytes still present

        // Cleanup
        QFile::remove(_account->cookieJarPath());
        QFile::remove(oldCookieJarPath);
    }
};

QTEST_APPLESS_MAIN(TestCookieJarMigration)
#include "testcookiejarmigration.moc"
