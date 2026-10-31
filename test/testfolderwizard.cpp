/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "account.h"
#include "foldermantestutils.h"
#include "folderwizard.h"
#include "syncenginetestutils.h"

using namespace OCC;
using namespace Qt::StringLiterals;

namespace OCC
{

class FolderWizardLocalPathTestAccess
{
public:
    static void applyChosenLocalFolder(FolderWizardLocalPath &page, const QString &localFolder, const QByteArray &bookmarkData, bool initialSelection)
    {
        page.applyChosenLocalFolder(localFolder, bookmarkData, initialSelection);
    }

    static void setEnteredLocalFolder(FolderWizardLocalPath &page, const QString &localFolder)
    {
        page._ui.localFolderLineEdit->setText(localFolder);
    }
};

}

class TestFolderWizard : public QObject
{
    Q_OBJECT

    FolderManTestHelper helper;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void chosenFolderKeepsItsBookmark()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        FolderWizardLocalPath page(createAccount());
        FolderWizardLocalPathTestAccess::applyChosenLocalFolder(page, dir.path(), "bookmark"_ba, true);
        QCOMPARE(page.securityScopedBookmarkData(), "bookmark"_ba);

        FolderWizardLocalPathTestAccess::setEnteredLocalFolder(page, dir.path() + u"/"_s);
        QCOMPARE(page.securityScopedBookmarkData(), "bookmark"_ba);
    }

    void editedFolderDropsTheBookmark()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        FolderWizardLocalPath page(createAccount());
        FolderWizardLocalPathTestAccess::applyChosenLocalFolder(page, dir.path(), "bookmark"_ba, true);
        FolderWizardLocalPathTestAccess::setEnteredLocalFolder(page, dir.path() + u"/other"_s);
        QVERIFY(page.securityScopedBookmarkData().isEmpty());
    }

    void cancellingInitialSelectionClosesWithoutBookmark()
    {
        FolderWizardLocalPath page(createAccount());
        QSignalSpy canceled(&page, &FolderWizardLocalPath::initialFolderSelectionCanceled);

        FolderWizardLocalPathTestAccess::applyChosenLocalFolder(page, {}, {}, true);
        QCOMPARE(canceled.count(), 1);
        QVERIFY(page.securityScopedBookmarkData().isEmpty());
    }

private:
    static AccountPtr createAccount()
    {
        auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setUrl(QUrl(u"http://example.de"_s));
        return account;
    }
};

QTEST_MAIN(TestFolderWizard)
#include "testfolderwizard.moc"
