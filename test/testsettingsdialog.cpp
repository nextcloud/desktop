/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include <QGroupBox>
#include <QLayout>

#include "settingsdialog.h"

using namespace OCC;

class TestSettingsDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void settingsPanelsUseNativeStyle()
    {
        SettingsDialog dialog(nullptr);
        const auto groupBoxPanelSelector = QStringLiteral("#generalGroupBox, #fileProviderGroupBox");
        const auto accountPanelSelector = QStringLiteral("#accountStatusPanel, #encryptionPanel");

        QVERIFY(!dialog.styleSheet().contains(groupBoxPanelSelector));
        QVERIFY(!dialog.styleSheet().contains(accountPanelSelector));
        QVERIFY(dialog.findChild<QGroupBox *>(QStringLiteral("settings_navigation_panel")));
    }

    void fileProviderDescriptionUsesPanelInset()
    {
        SettingsDialog dialog(nullptr);
        const auto controlRow = dialog.findChild<QLayout *>(QStringLiteral("fileProviderRow"));
        const auto descriptionRow = dialog.findChild<QLayout *>(QStringLiteral("fileProviderDescriptionRow"));
        QVERIFY(controlRow);
        QVERIFY(descriptionRow);

        const auto panelMargin = controlRow->contentsMargins().left();
        const auto descriptionMargins = descriptionRow->contentsMargins();
        QCOMPARE(descriptionMargins.left(), panelMargin);
        QCOMPARE(descriptionMargins.top(), 0);
        QCOMPARE(descriptionMargins.right(), panelMargin);
        QCOMPARE(descriptionMargins.bottom(), panelMargin);
    }
};

QTEST_MAIN(TestSettingsDialog)
#include "testsettingsdialog.moc"
