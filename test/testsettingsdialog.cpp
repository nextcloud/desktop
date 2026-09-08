/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settingsdialog.h"

#include <QApplication>
#include <QGroupBox>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTest>

class TestSettingsDialog : public QObject
{
    Q_OBJECT

    QPalette _originalPalette;

private Q_SLOTS:
    void initTestCase()
    {
        Q_INIT_RESOURCE(resources);
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        _originalPalette = QApplication::palette();
    }

    void cleanup()
    {
        QApplication::setPalette(_originalPalette);
    }

    void panelColors_data()
    {
        QTest::addColumn<QColor>("window");
        QTest::addColumn<QColor>("foreground");
        QTest::addColumn<QColor>("candidate");
        QTest::addColumn<QColor>("expected");

        QTest::newRow("white") << QColor(Qt::white) << QColor(Qt::black) << QColor(Qt::white) << QColor("#f7f7f7");
        QTest::newRow("almost-white") << QColor(Qt::white) << QColor(Qt::black) << QColor("#fdfdfd") << QColor("#f7f7f7");
        QTest::newRow("black") << QColor(Qt::black) << QColor(Qt::white) << QColor(Qt::black) << QColor("#0f0f0f");
        QTest::newRow("dark") << QColor("#202020") << QColor(Qt::white) << QColor("#202020") << QColor("#2d2d2d");
        QTest::newRow("preserve-light") << QColor(Qt::white) << QColor(Qt::black) << QColor("#f7f7f7") << QColor("#f7f7f7");
        QTest::newRow("preserve-dark") << QColor("#202020") << QColor(Qt::white) << QColor("#404040") << QColor("#404040");
    }

    void panelColors()
    {
        QFETCH(QColor, window);
        QFETCH(QColor, foreground);
        QFETCH(QColor, candidate);
        QFETCH(QColor, expected);

        auto palette = _originalPalette;
        palette.setColor(QPalette::Window, window);
        palette.setColor(QPalette::WindowText, foreground);
        palette.setColor(QPalette::Light, candidate);
        palette.setColor(QPalette::AlternateBase, candidate);
        QApplication::setPalette(palette);

        OCC::SettingsDialog dialog(nullptr);
        dialog.ensurePolished();
        const auto panel = dialog.findChild<QGroupBox *>(QStringLiteral("generalGroupBox"));
        const auto navigation = dialog.findChild<QScrollArea *>(QStringLiteral("settings_navigation_scroll"));
        QVERIFY(panel);
        QVERIFY(navigation);
        QCOMPARE(dialog.palette().color(QPalette::Window), window);
        QCOMPARE(panel->palette().color(QPalette::Window), expected);
        QCOMPARE(navigation->palette().color(QPalette::Window), expected);
    }

    void panelColorsFollowApplicationPalette()
    {
        OCC::SettingsDialog dialog(nullptr);
        dialog.ensurePolished();
        const auto panel = dialog.findChild<QGroupBox *>(QStringLiteral("generalGroupBox"));
        QVERIFY(panel);

        for (const auto dark : {false, true, false}) {
            const auto background = QColor(dark ? "#202020" : "#ffffff");
            const auto expected = QColor(dark ? "#2d2d2d" : "#f7f7f7");
            auto palette = _originalPalette;
            palette.setColor(QPalette::Window, background);
            palette.setColor(QPalette::WindowText, dark ? Qt::white : Qt::black);
            palette.setColor(QPalette::Light, background);
            palette.setColor(QPalette::AlternateBase, background);
            QApplication::setPalette(palette);

            QTRY_COMPARE(dialog.palette().color(QPalette::Window), background);
            QTRY_COMPARE(panel->palette().color(QPalette::Window), expected);
            QCOMPARE(panel->palette().color(QPalette::Inactive, QPalette::Window), expected);

            QEvent themeChange(QEvent::ThemeChange);
            QCoreApplication::sendEvent(&dialog, &themeChange);
            QCOMPARE(panel->palette().color(QPalette::Window), expected);

            OCC::SettingsDialog reopened(nullptr);
            reopened.ensurePolished();
            const auto reopenedPanel = reopened.findChild<QGroupBox *>(QStringLiteral("generalGroupBox"));
            QVERIFY(reopenedPanel);
            QCOMPARE(reopenedPanel->palette().color(QPalette::Window), expected);
        }
    }
};

QTEST_MAIN(TestSettingsDialog)
#include "testsettingsdialog.moc"
