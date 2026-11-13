/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include <QAction>
#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QMenu>
#include <QPalette>
#include <QScopeGuard>
#include <QSize>
#include <QTemporaryDir>

#include "systray.h"

#include "systraysynccontroltesthelper.h"

using namespace OCC;
using namespace Qt::StringLiterals;

class TestSystraySyncControlQt : public QObject
{
    Q_OBJECT

    SystraySyncControlTestHelper _helper;

    static QAction *pauseSyncAction(QMenu &menu, Systray *systray)
    {
        setupQtTrayContextMenu(&menu, systray);
        return menu.findChild<QAction *>(QStringLiteral("trayPauseSyncAction"));
    }

    static QAction *resumeSyncAction(const QMenu &menu)
    {
        return menu.findChild<QAction *>(QStringLiteral("trayResumeSyncAction"));
    }

    static QAction *contextMenuSyncNowAction(Systray *systray)
    {
        const auto menu = systray->contextMenu();
        if (!menu) {
            return nullptr;
        }
        // The context menu refreshes its actions right before being shown.
        Q_EMIT menu->aboutToShow();
        return menu->findChild<QAction *>(u"traySyncNowAction"_s);
    }

    static QAction *popupSyncNowAction(QMenu &menu, Systray *systray)
    {
        setupQtTrayContextMenu(&menu, systray);
        return menu.findChild<QAction *>(u"traySyncNowAction"_s);
    }

    /** @brief Adds a classic folder to a connected account; the folder deletes the account state when removed. */
    static Folder *addConnectedFolder(const QString &path)
    {
        auto account = Account::create();
        account->setUrl(QUrl(u"https://three.example.com"_s));
        account->setDavUser(u"carol"_s);
        account->setCredentials(new HttpCredentialsTest(u"carol"_s, u"secret"_s));
        return FolderMan::instance()->addFolder(new FakeAccountState(account), folderDefinition(path));
    }

    static QByteArray iconAlphaMask(const QIcon &icon)
    {
        const auto image = icon.pixmap(QSize(16, 16), QIcon::Normal, QIcon::Off).toImage().convertToFormat(QImage::Format_ARGB32);
        auto alphaMask = QByteArray{};
        alphaMask.reserve(image.width() * image.height());
        for (auto y = 0; y < image.height(); ++y) {
            for (auto x = 0; x < image.width(); ++x) {
                alphaMask.append(static_cast<char>(image.pixelColor(x, y).alpha()));
            }
        }
        return alphaMask;
    }

    static void verifyIconShape(const QAction *action, const QString &expectedIconPath)
    {
        QVERIFY(action);
        QVERIFY(!action->icon().isNull());

        const auto expectedIcon = QIcon{expectedIconPath};
        QVERIFY(!expectedIcon.isNull());
        QCOMPARE(iconAlphaMask(action->icon()), iconAlphaMask(expectedIcon));
    }

private Q_SLOTS:
    void initTestCase()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        QVERIFY(_helper.initialize());
    }

    void cleanupTestCase()
    {
        _helper.cleanup();
    }

    void menuIconsUseTheMenuPalette()
    {
        const auto iconColor = QColor{QStringLiteral("#f1e2d3")};
        auto darkPalette = QPalette{};
        darkPalette.setColor(QPalette::Base, Qt::black);
        darkPalette.setColor(QPalette::Window, Qt::black);
        darkPalette.setColor(QPalette::Text, iconColor);

        auto menu = QMenu{};
        menu.setPalette(darkPalette);
        QCOMPARE(nativeMenuIconPalette(&menu).color(QPalette::Active, QPalette::Text), iconColor);
    }

    void defaultWindowRequestsSettings()
    {
        const auto systray = Systray::instance();
        const auto settingsRequest = QSignalSpy(systray, &Systray::openSettings);

        systray->showWindow();

        QCOMPARE_EQ(settingsRequest.count(), 1);
    }

    void globalActionIsHiddenWithoutClassicFoldersAndTogglesAllFolders()
    {
        const auto systray = Systray::instance();

        // An account without classic folders is also the state used by a File Provider-only client.
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Unavailable);

        auto unavailableMenu = QMenu{};
        QVERIFY(!pauseSyncAction(unavailableMenu, systray));
        QVERIFY(!resumeSyncAction(unavailableMenu));

        QVERIFY(_helper.addClassicFolders());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);

        auto pauseMenu = QMenu{};
        const auto pauseAction = pauseSyncAction(pauseMenu, systray);
        QVERIFY(pauseAction);
        QVERIFY(!resumeSyncAction(pauseMenu));
        QCOMPARE(pauseAction->text(), Systray::tr("Pause sync for all"));
        pauseAction->trigger();

        QVERIFY(_helper.firstFolder()->syncPaused());
        QVERIFY(_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Resume);

        auto resumeMenu = QMenu{};
        QVERIFY(!pauseSyncAction(resumeMenu, systray));
        const auto resumeAction = resumeSyncAction(resumeMenu);
        QVERIFY(resumeAction);
        QCOMPARE(resumeAction->text(), Systray::tr("Resume sync for all"));
        resumeAction->trigger();

        QVERIFY(!_helper.firstFolder()->syncPaused());
        QVERIFY(!_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);
    }

    void syncControlActionsUseActionGlyphs()
    {
        const auto systray = Systray::instance();
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);

        auto pauseMenu = QMenu{};
        const auto pauseAction = pauseSyncAction(pauseMenu, systray);
        QVERIFY(pauseAction);
        verifyIconShape(pauseAction, QStringLiteral(":/client/theme/pause.svg"));
        pauseAction->trigger();

        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Resume);
        auto resumeMenu = QMenu{};
        setupQtTrayContextMenu(&resumeMenu, systray);
        QVERIFY(!resumeMenu.findChild<QAction *>(QStringLiteral("trayPauseSyncAction")));
        const auto actualResumeAction = resumeSyncAction(resumeMenu);
        QVERIFY(actualResumeAction);
        verifyIconShape(actualResumeAction, QStringLiteral(":/client/theme/play.svg"));
        actualResumeAction->trigger();

        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);
    }

    void partiallyPausedFoldersOfferPauseAndResume()
    {
        const auto systray = Systray::instance();

        _helper.firstFolder()->setSyncPaused(true);
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::PauseAndResume);

        auto resumeMenu = QMenu{};
        const auto pauseAction = pauseSyncAction(resumeMenu, systray);
        const auto resumeAction = resumeSyncAction(resumeMenu);
        QVERIFY(pauseAction);
        QVERIFY(resumeAction);
        QCOMPARE(pauseAction->text(), Systray::tr("Pause sync for all"));
        QCOMPARE(resumeAction->text(), Systray::tr("Resume sync for all"));
        resumeAction->trigger();

        QVERIFY(!_helper.firstFolder()->syncPaused());
        QVERIFY(!_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);

        _helper.firstFolder()->setSyncPaused(true);
        auto pauseMenu = QMenu{};
        const auto mixedPauseAction = pauseSyncAction(pauseMenu, systray);
        QVERIFY(mixedPauseAction);
        QVERIFY(resumeSyncAction(pauseMenu));
        mixedPauseAction->trigger();

        QVERIFY(_helper.firstFolder()->syncPaused());
        QVERIFY(_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Resume);
    }

    void initialFolderStatesShowWaitingToStart()
    {
        const auto firstFolder = _helper.firstFolder();
        const auto secondFolder = _helper.secondFolder();
        QVERIFY(firstFolder);
        QVERIFY(secondFolder);

        firstFolder->setSyncPaused(false);
        secondFolder->setSyncPaused(false);
        firstFolder->setSyncState(SyncResult::Undefined);
        secondFolder->setSyncState(SyncResult::NotYetStarted);

        auto overallStatus = SyncResult::Success;
        auto hasUnresolvedConflicts = true;
        auto *overallProgressInfo = static_cast<ProgressInfo *>(nullptr);
        FolderMan::trayOverallStatus({firstFolder, secondFolder}, &overallStatus, &hasUnresolvedConflicts, &overallProgressInfo);

        QCOMPARE(overallStatus, SyncResult::NotYetStarted);
        QCOMPARE(FolderMan::trayTooltipStatusString(overallStatus, hasUnresolvedConflicts, false, overallProgressInfo),
                 QStringLiteral("Waiting to start syncing."));
    }

    void contextMenuSyncNowSchedulesSyncableFolders()
    {
        const auto systray = Systray::instance();
        const auto folderMan = FolderMan::instance();

        // The helper's accounts never connect, so none of their folders can sync.
        QVERIFY(!systray->canSyncNow());
        const auto hiddenAction = contextMenuSyncNowAction(systray);
        QVERIFY(hiddenAction);
        QVERIFY(!hiddenAction->isVisible());

        QTemporaryDir folderDir;
        QVERIFY(folderDir.isValid());
        const auto folder = addConnectedFolder(folderDir.path());
        QVERIFY(folder);
        const auto removeFolder = qScopeGuard([folderMan, folder] {
            folderMan->removeFolder(folder);
        });
        QVERIFY(systray->canSyncNow());
        QVERIFY(!folderMan->scheduleQueue().contains(folder));

        const auto syncNowAction = contextMenuSyncNowAction(systray);
        QVERIFY(syncNowAction);
        QVERIFY(syncNowAction->isVisible());
        syncNowAction->trigger();
        syncNowAction->trigger();

        // Clicking again does not queue the folder twice.
        QCOMPARE(folderMan->scheduleQueue().count(folder), 1);
        QCOMPARE(folderMan->scheduleQueue().first(), folder);
        QVERIFY(!folderMan->scheduleQueue().contains(_helper.firstFolder()));
        QVERIFY(!folderMan->scheduleQueue().contains(_helper.secondFolder()));

        folder->setSyncPaused(true);
        QVERIFY(!systray->canSyncNow());
        QVERIFY(!contextMenuSyncNowAction(systray)->isVisible());
    }

    void popupSyncNowSchedulesSyncableFolders()
    {
        const auto systray = Systray::instance();
        const auto folderMan = FolderMan::instance();

        auto hiddenMenu = QMenu{};
        QVERIFY(!popupSyncNowAction(hiddenMenu, systray));

        QTemporaryDir folderDir;
        QVERIFY(folderDir.isValid());
        const auto folder = addConnectedFolder(folderDir.path());
        QVERIFY(folder);
        const auto removeFolder = qScopeGuard([folderMan, folder] {
            folderMan->removeFolder(folder);
        });

        auto menu = QMenu{};
        const auto syncNowAction = popupSyncNowAction(menu, systray);
        QVERIFY(syncNowAction);
        QCOMPARE(syncNowAction->text(), Systray::tr("Sync now"));
        QVERIFY(!syncNowAction->icon().isNull());
        syncNowAction->trigger();

        QCOMPARE(folderMan->scheduleQueue().count(folder), 1);
        QVERIFY(!folderMan->scheduleQueue().contains(_helper.firstFolder()));
        QVERIFY(!folderMan->scheduleQueue().contains(_helper.secondFolder()));

        folder->setSyncPaused(true);
        auto pausedMenu = QMenu{};
        QVERIFY(!popupSyncNowAction(pausedMenu, systray));
    }
};

QTEST_MAIN(TestSystraySyncControlQt)
#include "testsystraysynccontrolqt.moc"
