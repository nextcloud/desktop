/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "accountmanager.h"
#include "configfile.h"
#include "foldermantestutils.h"
#include "syncenginetestutils.h"
#include "systray.h"
#include "tray/usermodel.h"

#include <QAction>
#include <QApplication>
#include <QFont>
#include <QMenu>
#include <QPointer>
#include <QScreen>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace OCC;
using namespace Qt::StringLiterals;

class TestTrayAccountMenuQt : public QObject
{
    Q_OBJECT

    QTemporaryDir _configDir;
    FolderManTestHelper _folderManHelper;
    AccountState *_accountState = nullptr;
    int _activityRequestCount = 0;

    bool setConnectionStatus(const ConnectionValidator::Status status)
    {
        return QMetaObject::invokeMethod(_accountState,
                                         "slotConnectionValidatorResult",
                                         Q_ARG(OCC::ConnectionValidator::Status, status),
                                         Q_ARG(QStringList, QStringList{}));
    }

    static QMenu *accountMenu(const QMenu &menu)
    {
        return menu.actions().isEmpty() ? nullptr : menu.actions().first()->menu();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QApplication::setStyle(u"Fusion"_s);
        QApplication::setLayoutDirection(Qt::LeftToRight);
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
        QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
        constexpr auto testFontPixelSize = 12;
        auto font = QApplication::font();
        font.setPixelSize(testFontPixelSize);
        QApplication::setFont(font);

        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(_configDir.isValid());
        ConfigFile::setConfDir(_configDir.path());
        Systray::instance()->create();
        auto account = Account::create();
        account->setUrl(QUrl(u"https://cloud.example.com"_s));
        account->setDavUser(u"alice"_s);
        account->setCapabilities({{u"activity"_s, QVariantMap{}}});
        _activityRequestCount = 0;
        const auto network = new FakeQNAM({});
        network->setOverride([this, network](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (request.url().path().endsWith(u"/apps/activity/api/v2/activity"_s)) {
                ++_activityRequestCount;
            }
            return new FakeErrorReply(operation, request, network, 404);
        });
        account->setCredentials(new FakeCredentials(network));
        _accountState = AccountManager::instance()->addAccount(account);
        QVERIFY(_accountState);
        Q_EMIT AccountManager::instance()->accountListInitialized();
        QCOMPARE(UserModel::instance()->rowCount(), 1);
    }

    void init()
    {
        _activityRequestCount = 0;
        QVERIFY(setConnectionStatus(ConnectionValidator::Timeout));
        QVERIFY(!UserModel::instance()->data(UserModel::instance()->index(0), UserModel::IsConnectedRole).toBool());
    }

    void cleanup()
    {
        Systray::instance()->setTrayContextMenuVisible(false);
    }

    void cleanupTestCase()
    {
        AccountManager::instance()->removeAccountState(_accountState);
        _accountState = nullptr;
    }

    void disconnectedSubmenuDoesNotDuplicateActions()
    {
        auto menu = QMenu{};
        setupQtTrayContextMenu(&menu, Systray::instance());
        QVERIFY(QMetaObject::invokeMethod(&menu, "aboutToShow"));
        const auto submenu = accountMenu(menu);
        QVERIFY(submenu);
        QCOMPARE(submenu->actions().size(), 3);
        QCOMPARE(submenu->actions().at(0)->text(), QCoreApplication::translate("TrayFoldersMenuButton", "Local folder"));
        QVERIFY(submenu->actions().at(1)->isSeparator());
        QCOMPARE(submenu->actions().at(2)->text(), QCoreApplication::translate("OCC::AccountSettings", "Log in"));

        const auto initialActions = submenu->actions();
        const auto openingSignal = QSignalSpy(submenu, &QMenu::aboutToShow);
        constexpr auto openingCount = 3;
        for (auto opening = 0; opening < openingCount; ++opening) {
            submenu->popup(QPoint{});
            QCOMPARE(openingSignal.count(), opening + 1);
            QCOMPARE(submenu->actions().size(), initialActions.size());
            QCOMPARE(submenu->actions(), initialActions);
            submenu->hide();
        }
        QCOMPARE(_activityRequestCount, 0);
    }

    void openingPreservesPrepopulatedActions_data()
    {
        QTest::addColumn<bool>("connected");
        QTest::newRow("disconnected") << false;
        QTest::newRow("connected") << true;
    }

    void openingPreservesPrepopulatedActions()
    {
        QFETCH(bool, connected);
        if (connected) {
            QVERIFY(setConnectionStatus(ConnectionValidator::Connected));
            QVERIFY(UserModel::instance()->data(UserModel::instance()->index(0), UserModel::IsConnectedRole).toBool());
        }

        auto menu = QMenu{};
        setupQtTrayContextMenu(&menu, Systray::instance());
        QVERIFY(QMetaObject::invokeMethod(&menu, "aboutToShow"));
        const auto submenu = accountMenu(menu);
        QVERIFY(submenu);
        // AppIndicator needs contents before aboutToShow and reports the submenu as hidden.
        QVERIFY(!submenu->isEmpty());
        QVERIFY(!submenu->isVisible());
        const auto initialActions = submenu->actions();
        const auto firstAction = QPointer<QAction>(initialActions.first());
        const auto destroyed = QSignalSpy(firstAction.data(), &QObject::destroyed);

        QVERIFY(QMetaObject::invokeMethod(submenu, "aboutToShow"));
        QVERIFY(firstAction);
        QCOMPARE(destroyed.count(), 0);
        QCOMPARE(submenu->actions(), initialActions);
        QCOMPARE(_activityRequestCount, connected ? 1 : 0);
    }

    void submenusStayConnected_data()
    {
        QTest::addColumn<bool>("openToLeft");
        QTest::addColumn<bool>("wideStyle");
        QTest::newRow("compact-right") << false << false;
        QTest::newRow("compact-left") << true << false;
        QTest::newRow("wide-style-right") << false << true;
        QTest::newRow("wide-style-left") << true << true;
    }

    void submenusStayConnected()
    {
        QFETCH(bool, openToLeft);
        QFETCH(bool, wideStyle);

        auto menu = QMenu{};
        setupQtTrayContextMenu(&menu, Systray::instance());
        if (wideStyle) {
            constexpr auto rootItemPadding = 80;
            menu.setStyleSheet(u"QMenu::item { padding: 0px %1px; }"_s.arg(rootItemPadding));
        }

        constexpr auto screenEdgeMargin = 10;
        constexpr auto popupTopMargin = 50;
        const auto screen = QApplication::primaryScreen();
        QVERIFY(screen);
        const auto availableGeometry = screen->availableGeometry();
        const auto popupX = openToLeft ? availableGeometry.right() - menu.sizeHint().width() - screenEdgeMargin : availableGeometry.left() + screenEdgeMargin;
        menu.popup(QPoint{popupX, availableGeometry.top() + popupTopMargin});
        QTRY_VERIFY(menu.isVisible());

        const auto submenu = accountMenu(menu);
        QVERIFY(submenu);
        if (wideStyle) {
            constexpr auto submenuItemPadding = 140;
            submenu->setStyleSheet(u"QMenu::item { padding: 0px %1px; }"_s.arg(submenuItemPadding));
        }
        QVERIFY(menu.sizeHint().width() + submenu->sizeHint().width() + 2 * screenEdgeMargin < availableGeometry.width());
        menu.setActiveAction(submenu->menuAction());
        QTest::keyClick(&menu, Qt::Key_Right);
        QTRY_VERIFY(submenu->isVisible());

        const auto parentGeometry = menu.geometry();
        const auto submenuGeometry = submenu->geometry();
        const auto gap = openToLeft ? parentGeometry.left() - submenuGeometry.right() - 1 : submenuGeometry.left() - parentGeometry.right() - 1;
        QVERIFY2(gap <= 0, qPrintable(u"Submenu is separated from its parent by %1 pixels."_s.arg(gap)));
        QCOMPARE(submenuGeometry.center().x() < parentGeometry.center().x(), openToLeft);
        QCOMPARE(menu.width(), menu.sizeHint().width());
        QCOMPARE(submenu->width(), submenu->sizeHint().width());

        QTest::keyClick(submenu, Qt::Key_Escape);
        QTRY_VERIFY(!submenu->isVisible());
        QTest::keyClick(&menu, Qt::Key_Escape);
        QTRY_VERIFY(!menu.isVisible());
    }

    void connectionChangesRefreshOnlyVisibleSubmenus()
    {
        auto menu = QMenu{};
        setupQtTrayContextMenu(&menu, Systray::instance());
        QVERIFY(QMetaObject::invokeMethod(&menu, "aboutToShow"));
        const auto submenu = accountMenu(menu);
        QVERIFY(submenu);
        const auto initialActions = submenu->actions();
        const auto firstAction = QPointer<QAction>(initialActions.first());

        QVERIFY(setConnectionStatus(ConnectionValidator::Connected));
        QVERIFY(firstAction);
        QCOMPARE(submenu->actions(), initialActions);
        QVERIFY(QMetaObject::invokeMethod(submenu, "aboutToShow"));
        QCOMPARE(submenu->actions(), initialActions);

        submenu->popup(QPoint{});
        QVERIFY(submenu->isVisible());
        QVERIFY(setConnectionStatus(ConnectionValidator::Timeout));
        QVERIFY(!firstAction);
        QCOMPARE(submenu->actions().size(), 3);
        QCOMPARE(submenu->actions().last()->text(), QCoreApplication::translate("OCC::AccountSettings", "Log in"));

        QVERIFY(setConnectionStatus(ConnectionValidator::Connected));
        const auto searchText = QCoreApplication::translate("TrayAccountPopup", "Search");
        const auto loginText = QCoreApplication::translate("OCC::AccountSettings", "Log in");
        auto hasSearch = false;
        for (const auto action : submenu->actions()) {
            QVERIFY(action->text() != loginText);
            hasSearch |= action->text() == searchText;
        }
        QVERIFY(hasSearch);
    }
};

QTEST_MAIN(TestTrayAccountMenuQt)
#include "testtrayaccountmenuqt.moc"
