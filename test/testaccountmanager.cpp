/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include <QStandardPaths>
#include <QUrl>

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "logger.h"
#include "syncenginetestutils.h"

using namespace OCC;
using namespace Qt::StringLiterals;

class TestAccountManager : public QObject
{
    Q_OBJECT

private:
    //! @brief The account state registered with AccountManager for the current
    //! test. Set by addTestAccount() and cleared by the cleanup() slot.
    AccountState *_accountState = nullptr;

    //! @brief Creates an account with the given server URL and DAV user, registers
    //! it with AccountManager, and stores the resulting AccountState in
    //! _accountState so that cleanup() can remove it.
    AccountState *addTestAccount(const QString &serverUrl, const QString &davUser)
    {
        auto account = Account::create();
        account->setUrl(QUrl(serverUrl));
        account->setDavUser(davUser);
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        _accountState = AccountManager::instance()->addAccount(account);
        return _accountState;
    }

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    //! @brief Removes the test account from AccountManager after each test
    //! function, including every data-driven row.
    void cleanup()
    {
        if (_accountState) {
            AccountManager::instance()->removeAccountState(_accountState);
            _accountState = nullptr;
        }
    }

    // ---------------------------------------------------------------------------
    // accountFromUserId – plain ASCII domain
    // ---------------------------------------------------------------------------

    //! @brief Verifies that accountFromUserId finds an account whose server URL
    //! uses a plain ASCII domain when queried with the matching userId string.
    void testAccountFromUserId_asciiDomain_matchesByExactId()
    {
        QVERIFY(addTestAccount(u"https://cloud.example.com"_s, u"alice"_s));

        const auto found = AccountManager::instance()->accountFromUserId(u"alice@cloud.example.com"_s);
        QVERIFY(found);
        QCOMPARE(found->account()->davUser(), u"alice"_s);
    }

    //! @brief Verifies that accountFromUserId returns a null pointer for an
    //! entirely unknown userId so that a mismatch is detectable.
    void testAccountFromUserId_unknownId_returnsNull()
    {
        QVERIFY(addTestAccount(u"https://cloud.example.com"_s, u"alice"_s));

        const auto notFound = AccountManager::instance()->accountFromUserId(u"bob@other.example.com"_s);
        QVERIFY(!notFound);
    }

    // ---------------------------------------------------------------------------
    // accountFromUserId – IDN / Punycode domain normalisation
    // ---------------------------------------------------------------------------

    void testAccountFromUserId_idnDomain_data()
    {
        QTest::addColumn<QString>("serverUrl");
        QTest::addColumn<QString>("davUser");
        QTest::addColumn<QString>("incomingUserId");
        QTest::addColumn<bool>("shouldMatch");

        const auto unicodeHost = u"cloud.münchen.example"_s;
        const auto punycodeHost = QString::fromLatin1(QUrl::toAce(unicodeHost.toUtf8()));
        const auto mismatchedPunycodeHost = QString::fromLatin1(QUrl::toAce(u"cloud.zürich.example"_s.toUtf8()));
        QVERIFY2(!punycodeHost.isEmpty(), "The Unicode test host must convert to Punycode");
        QVERIFY2(!mismatchedPunycodeHost.isEmpty(), "The mismatched Unicode test host must convert to Punycode");

        // Account configured with the Unicode form of an IDN hostname.
        // The nc://open/ link may arrive with the corresponding ACE/Punycode form.
        QTest::newRow("unicode server url, unicode userid")
            << u"https://cloud.münchen.example"_s
            << u"testuser"_s
            << u"testuser@cloud.münchen.example"_s
            << true;

        QTest::newRow("unicode server url, punycode userid")
            << u"https://cloud.münchen.example"_s
            << u"testuser"_s
            << u"testuser@"_s + punycodeHost
            << true;

        // Account configured with the ACE/Punycode form (as reported in the bug):
        // QUrl::host() decodes it to Unicode internally, so the stored host is
        // Unicode regardless. Both userId forms must still match.
        QTest::newRow("punycode server url, punycode userid")
            << u"https://"_s + punycodeHost
            << u"testuser"_s
            << u"testuser@"_s + punycodeHost
            << true;

        QTest::newRow("punycode server url, unicode userid")
            << u"https://"_s + punycodeHost
            << u"testuser"_s
            << u"testuser@cloud.münchen.example"_s
            << true;

        // A Punycode userid for a completely different domain must not match.
        QTest::newRow("punycode server url, mismatched punycode userid")
            << u"https://"_s + punycodeHost
            << u"testuser"_s
            << u"testuser@"_s + mismatchedPunycodeHost
            << false;

        // Non-IDN ASCII domain must still work normally.
        QTest::newRow("ascii server url, ascii userid")
            << u"https://nextcloud.example.com"_s
            << u"testuser"_s
            << u"testuser@nextcloud.example.com"_s
            << true;
    }

    //! @brief Verifies that accountFromUserId correctly matches (or rejects)
    //! accounts when the incoming userId contains either the Unicode or the
    //! ACE/Punycode form of an internationalised domain name. This guards against
    //! the regression where nc://open/ links with a Punycode host failed to find
    //! an account whose stored URL was canonicalised to Unicode by QUrl.
    void testAccountFromUserId_idnDomain()
    {
        QFETCH(QString, serverUrl);
        QFETCH(QString, davUser);
        QFETCH(QString, incomingUserId);
        QFETCH(bool, shouldMatch);

        QVERIFY(addTestAccount(serverUrl, davUser));

        const auto found = AccountManager::instance()->accountFromUserId(incomingUserId);
        if (shouldMatch) {
            QVERIFY2(found, qPrintable(u"Expected to find account for userId '%1' but got null"_s.arg(incomingUserId)));
            QCOMPARE(found->account()->davUser(), davUser);
        } else {
            QVERIFY2(!found, qPrintable(u"Expected no account for userId '%1' but found one"_s.arg(incomingUserId)));
        }
    }

    // ---------------------------------------------------------------------------
    // accountFromUserId – IDN domain with non-standard port
    // ---------------------------------------------------------------------------

    //! @brief Verifies that accountFromUserId handles IDN hostnames correctly
    //! when the server URL also specifies a non-standard port.
    void testAccountFromUserId_idnDomainWithPort_data()
    {
        QTest::addColumn<QString>("serverUrl");
        QTest::addColumn<QString>("davUser");
        QTest::addColumn<QString>("incomingUserId");
        QTest::addColumn<bool>("shouldMatch");

        const auto unicodeHost = u"cloud.münchen.example"_s;
        const auto punycodeHost = QString::fromLatin1(QUrl::toAce(unicodeHost.toUtf8()));
        QVERIFY2(!punycodeHost.isEmpty(), "The Unicode test host must convert to Punycode");

        QTest::newRow("unicode server url with port, punycode userid with port")
            << u"https://"_s + unicodeHost + u":8443"_s
            << u"testuser"_s
            << u"testuser@"_s + punycodeHost + u":8443"_s
            << true;

        QTest::newRow("punycode server url with port, unicode userid with port")
            << u"https://"_s + punycodeHost + u":8443"_s
            << u"testuser"_s
            << u"testuser@"_s + unicodeHost + u":8443"_s
            << true;
    }

    //! @brief Companion to testAccountFromUserId_idnDomain covering the port
    //! component of the userId so that port-qualified IDN accounts can be located.
    void testAccountFromUserId_idnDomainWithPort()
    {
        QFETCH(QString, serverUrl);
        QFETCH(QString, davUser);
        QFETCH(QString, incomingUserId);
        QFETCH(bool, shouldMatch);

        QVERIFY(addTestAccount(serverUrl, davUser));

        const auto found = AccountManager::instance()->accountFromUserId(incomingUserId);
        if (shouldMatch) {
            QVERIFY2(found, qPrintable(u"Expected to find account for userId '%1' but got null"_s.arg(incomingUserId)));
            QCOMPARE(found->account()->davUser(), davUser);
        } else {
            QVERIFY2(!found, qPrintable(u"Expected no account for userId '%1' but found one"_s.arg(incomingUserId)));
        }
    }
};

QTEST_GUILESS_MAIN(TestAccountManager)
#include "testaccountmanager.moc"
