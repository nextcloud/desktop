/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NATIVESCREENSHOTCAPTUREUTILS_H
#define NATIVESCREENSHOTCAPTUREUTILS_H

#include <QString>

class QDialog;
class QWidget;

namespace OCC {
class IgnoreListEditor;
class SettingsDialog;
class UiScreenshotOutput;

namespace UiScreenshots {

/** @brief Identifies a production Settings page without translated labels. */
enum class SettingsPage {
    User,
    General,
    Advanced,
    Info,
};

/** @brief Triggers real Settings actions until @p page is the current production widget type. */
[[nodiscard]] bool selectSettingsPage(SettingsDialog *dialog, SettingsPage page, QString *error);

/** @brief Opens and returns the Connection Settings dialog through its production Account Settings slot. */
[[nodiscard]] QDialog *openNetworkSettingsDialog(SettingsDialog *dialog, QString *error);

/** @brief Opens and fills the ignored-files editor through its production Advanced Settings slot. */
[[nodiscard]] IgnoreListEditor *openIgnoreListEditor(SettingsDialog *dialog, QString *error);

/** @brief Activates nested widget layouts and requests painting before a queued settle turn. */
void activateWidgetLayouts(QWidget *widget);

/** @brief Grabs a production widget and atomically writes an allowlisted PNG. */
[[nodiscard]] bool captureWidget(UiScreenshotOutput &output, QWidget *widget, const QString &fileName, QString *error);

}
}

#endif // NATIVESCREENSHOTCAPTUREUTILS_H
