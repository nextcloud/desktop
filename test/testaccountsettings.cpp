/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QApplication>
#include <QDialog>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QtTest>

#include "account.h"
#include "foldermantestutils.h"
#include "logger.h"
#include "testhelper.h"

#include "accountsettings.h"

using namespace OCC;

class TestAccountSettings : public QObject
{
    Q_OBJECT

    FolderManTestHelper helper;

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void test_whenAccountStateIsNotConnected_doesNotCrash()
    {
        auto account = Account::create();
        auto accountState = new FakeAccountState(account);
        accountState->setStateForTesting(OCC::AccountState::SignedOut);
        QCOMPARE_EQ(accountState->state(), OCC::AccountState::SignedOut);
        AccountSettings a(accountState);
    }

    void test_whenAccountStateIsConnected_doesNotCrash()
    {
        // this occurred because ConnectionValidator used to set the account
        // inside a Account's _e2e member, instead of letting Account itself
        // do that.

        auto account = Account::create();
        auto accountState = new FakeAccountState(account);
        QCOMPARE_EQ(accountState->state(), OCC::AccountState::Connected);
        AccountSettings a(accountState);
    }

    void test_mnemonicDialog_wrapsLongMnemonic()
    {
        auto account = Account::create();
        auto accountState = new FakeAccountState(account);
        accountState->setStateForTesting(AccountState::SignedOut);
        AccountSettings accountSettings(accountState);

        const auto mnemonic = QStringLiteral(
            "squirrel purchase favorite document remember solution "
            "language discover umbrella tomorrow exercise mountain");

        auto dialogFound = false;
        auto textEditFound = false;
        auto readOnly = false;
        auto displayedMnemonic = QString{};
        auto lineWrapMode = QTextEdit::NoWrap;
        auto horizontalScrollBarPolicy = Qt::ScrollBarAsNeeded;
        auto initialDialogWidth = 0;
        auto resizedDialogWidth = 0;
        auto initialTextEditWidth = 0;
        auto resizedTextEditWidth = 0;
        auto visualLineCount = 0;

        QMetaObject::invokeMethod(this, [&] {
            const auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());

            if (!dialog) {
                QApplication::closeAllWindows();
                return;
            }

            dialogFound = true;
            initialDialogWidth = dialog->width();

            const auto mnemonicTextEdit = dialog->findChild<QTextEdit *>(QStringLiteral("mnemonicTextEdit"));

            if (mnemonicTextEdit) {
                textEditFound = true;
                initialTextEditWidth = mnemonicTextEdit->width();
            }

            dialog->resize(initialDialogWidth / 2, dialog->height());

            QMetaObject::invokeMethod(dialog, [&, dialog, mnemonicTextEdit] {
                resizedDialogWidth = dialog->width();

                if (mnemonicTextEdit) {
                    resizedTextEditWidth = mnemonicTextEdit->width();
                    readOnly = mnemonicTextEdit->isReadOnly();
                    displayedMnemonic = mnemonicTextEdit->toPlainText();
                    lineWrapMode = mnemonicTextEdit->lineWrapMode();
                    horizontalScrollBarPolicy = mnemonicTextEdit->horizontalScrollBarPolicy();

                    const auto textLayout = mnemonicTextEdit->document()->firstBlock().layout();

                    if (textLayout) {
                        visualLineCount = textLayout->lineCount();
                    }
                }

                dialog->accept();
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);

        const auto invoked = QMetaObject::invokeMethod(&accountSettings, "displayMnemonic", Qt::DirectConnection, Q_ARG(QString, mnemonic));

        QVERIFY(invoked);
        QVERIFY(dialogFound);
        QVERIFY(textEditFound);
        QVERIFY(readOnly);
        QCOMPARE(displayedMnemonic, mnemonic);
        QCOMPARE(lineWrapMode, QTextEdit::WidgetWidth);
        QCOMPARE(horizontalScrollBarPolicy, Qt::ScrollBarAlwaysOff);
        QVERIFY(resizedDialogWidth < initialDialogWidth);
        QVERIFY(resizedTextEditWidth < initialTextEditWidth);
        QVERIFY(visualLineCount > 1);
    }
};

QTEST_MAIN(TestAccountSettings)
#include "testaccountsettings.moc"
