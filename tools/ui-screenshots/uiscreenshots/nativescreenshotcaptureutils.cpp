/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nativescreenshotcaptureutils.h"

#include "accountsettings.h"
#include "advancedsettings.h"
#include "generalsettings.h"
#include "ignorelisteditor.h"
#include "ignorelisttablewidget.h"
#include "infosettings.h"
#include "settingsdialog.h"
#include "uiscreenshotoutput.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QLayout>
#include <QMetaObject>
#include <QWidget>

namespace OCC::UiScreenshots {

namespace {

bool isCurrentSettingsPage(SettingsDialog *dialog, const SettingsPage page)
{
    auto *currentPage = dialog->currentPage();
    switch (page) {
    case SettingsPage::User:
        return qobject_cast<AccountSettings *>(currentPage) != nullptr;
    case SettingsPage::General:
        return qobject_cast<GeneralSettings *>(currentPage) != nullptr;
    case SettingsPage::Advanced:
        return qobject_cast<AdvancedSettings *>(currentPage) != nullptr;
    case SettingsPage::Info:
        return qobject_cast<InfoSettings *>(currentPage) != nullptr;
    }
    Q_UNREACHABLE();
    return false;
}

QString settingsPageName(const SettingsPage page)
{
    switch (page) {
    case SettingsPage::User:
        return QStringLiteral("User Settings");
    case SettingsPage::General:
        return QStringLiteral("General Settings");
    case SettingsPage::Advanced:
        return QStringLiteral("Advanced Settings");
    case SettingsPage::Info:
        return QStringLiteral("Info Settings");
    }
    Q_UNREACHABLE();
    return {};
}

}

bool selectSettingsPage(SettingsDialog *dialog, const SettingsPage page, QString *error)
{
    if (!dialog) {
        if (error) {
            *error = QStringLiteral("Cannot select a page on a null Settings dialog.");
        }
        return false;
    }
    const auto actionGroup = dialog->findChild<QActionGroup *>(QString{}, Qt::FindDirectChildrenOnly);
    if (!actionGroup) {
        if (error) {
            *error = QStringLiteral("Could not find the production Settings action group.");
        }
        return false;
    }

    for (auto *action : actionGroup->actions()) {
        action->trigger();
        if (isCurrentSettingsPage(dialog, page)) {
            return true;
        }
    }
    if (error) {
        *error = QStringLiteral("Could not select the production %1 page.").arg(settingsPageName(page));
    }
    return false;
}

QDialog *openNetworkSettingsDialog(SettingsDialog *dialog, QString *error)
{
    if (!selectSettingsPage(dialog, SettingsPage::User, error)) {
        return nullptr;
    }
    auto *accountSettings = qobject_cast<AccountSettings *>(dialog->currentPage());
    if (!accountSettings) {
        if (error) {
            *error = QStringLiteral("The production User Settings page is not an AccountSettings widget.");
        }
        return nullptr;
    }

    const auto existingDialogs = accountSettings->findChildren<QDialog *>(QString{}, Qt::FindDirectChildrenOnly);
    if (!QMetaObject::invokeMethod(accountSettings, "showConnectionSettingsDialog", Qt::DirectConnection)) {
        if (error) {
            *error = QStringLiteral("Could not invoke the production Connection Settings action.");
        }
        return nullptr;
    }

    const auto dialogs = accountSettings->findChildren<QDialog *>(QString{}, Qt::FindDirectChildrenOnly);
    for (auto *candidate : dialogs) {
        if (!existingDialogs.contains(candidate)) {
            return candidate;
        }
    }
    if (error) {
        *error = QStringLiteral("The production Connection Settings action did not open a child dialog.");
    }
    return nullptr;
}

IgnoreListEditor *openIgnoreListEditor(SettingsDialog *dialog, QString *error)
{
    if (!selectSettingsPage(dialog, SettingsPage::Advanced, error)) {
        return nullptr;
    }
    auto *advancedSettings = qobject_cast<AdvancedSettings *>(dialog->currentPage());
    if (!advancedSettings) {
        if (error) {
            *error = QStringLiteral("The production Advanced Settings page is not an AdvancedSettings widget.");
        }
        return nullptr;
    }
    if (!QMetaObject::invokeMethod(advancedSettings, "slotIgnoreFilesEditor", Qt::DirectConnection)) {
        if (error) {
            *error = QStringLiteral("Could not invoke the production ignored-files action.");
        }
        return nullptr;
    }

    const auto editor = advancedSettings->findChild<IgnoreListEditor *>(QString{}, Qt::FindDirectChildrenOnly);
    if (!editor) {
        if (error) {
            *error = QStringLiteral("The production ignored-files action did not open an IgnoreListEditor.");
        }
        return nullptr;
    }
    const auto table = editor->findChild<IgnoreListTableWidget *>();
    if (!table) {
        if (error) {
            *error = QStringLiteral("Could not find the production ignored-files table.");
        }
        editor->close();
        return nullptr;
    }

    table->slotRemoveAllItems();
    table->addPattern(QStringLiteral(".csync_journal.db*"), false, true);
    table->addPattern(QStringLiteral("._sync_*.db*"), false, true);
    table->addPattern(QStringLiteral(".sync_*.db*"), false, true);
    table->addPattern(QStringLiteral("*.part"), true, false);
    table->addPattern(QStringLiteral("Temporary files"), false, false);
    return editor;
}

void activateWidgetLayouts(QWidget *widget)
{
    if (!widget) {
        return;
    }
    widget->ensurePolished();
    if (auto *layout = widget->layout()) {
        layout->activate();
    }
    const auto children = widget->findChildren<QWidget *>();
    for (auto *child : children) {
        child->ensurePolished();
        if (auto *layout = child->layout()) {
            layout->activate();
        }
        child->update();
    }
    widget->updateGeometry();
    widget->update();
}

bool captureWidget(UiScreenshotOutput &output, QWidget *widget, const QString &fileName, QString *error)
{
    if (!widget) {
        if (error) {
            *error = QStringLiteral("Cannot capture a null native widget for %1.").arg(fileName);
        }
        return false;
    }
    const auto pixmap = widget->grab();
    if (pixmap.isNull() || pixmap.width() <= 0 || pixmap.height() <= 0) {
        if (error) {
            *error = QStringLiteral("Native widget grab was empty for %1.").arg(fileName);
        }
        return false;
    }
    return output.writePng(fileName, pixmap.toImage(), error);
}

}
