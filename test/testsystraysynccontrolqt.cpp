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
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QMenu>
#include <QPalette>
#include <QSize>

#include "systray.h"

#include "systraysynccontroltesthelper.h"

using namespace OCC;

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

    static QAction *findAction(const QMenu &menu, const QString &text)
    {
        for (const auto action : menu.actions()) {
            if (action->text() == text) {
                return action;
            }
            if (const auto subMenu = action->menu()) {
                if (const auto childAction = findAction(*subMenu, text)) {
                    return childAction;
                }
            }
        }
        return nullptr;
    }

    static void verifyIconColor(const QAction *action, const QColor &expectedColor)
    {
        QVERIFY(action);
        QVERIFY(!action->icon().isNull());

        const auto image = action->icon().pixmap(QSize(16, 16), QIcon::Normal, QIcon::Off).toImage().convertToFormat(QImage::Format_ARGB32);
        auto foundSolidPixel = false;
        for (auto y = 0; y < image.height(); ++y) {
            for (auto x = 0; x < image.width(); ++x) {
                const auto pixelColor = image.pixelColor(x, y);
                if (pixelColor.alpha() != 255) {
                    continue;
                }

                foundSolidPixel = true;
                QCOMPARE(pixelColor.red(), expectedColor.red());
                QCOMPARE(pixelColor.green(), expectedColor.green());
                QCOMPARE(pixelColor.blue(), expectedColor.blue());
            }
        }
        QVERIFY(foundSolidPixel);
    }

private Q_SLOTS:
    void initTestCase()
    {
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
        setupQtTrayContextMenu(&menu, Systray::instance());

        verifyIconColor(findAction(menu, Systray::tr("Settings")), iconColor);
        verifyIconColor(findAction(menu, QCoreApplication::translate("TrayFoldersMenuButton", "Local folder")), iconColor);
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
};

QTEST_MAIN(TestSystraySyncControlQt)
#include "testsystraysynccontrolqt.moc"
