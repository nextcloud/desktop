/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class QWindow;

namespace OCC::Mac::SandboxFilePicker
{

/**
 * Select where to save a file with the native macOS save panel.
 *
 * The panel is shown as a sheet of parentWindow and only accepts file names with fileExtension.
 * completionHandler receives the chosen path, or an empty string if the user cancels.
 * The sandbox access to the path ends when completionHandler returns, so write the file in it.
 */
void selectFileToSave(QWindow *parentWindow,
                      const QString &caption,
                      const QString &directory,
                      const QString &fileExtension,
                      std::function<void(const QString &)> completionHandler);

/**
 * Select an existing file with the native macOS open panel.
 *
 * The panel is shown as a sheet of parentWindow and only offers files with one of fileExtensions.
 * completionHandler receives the chosen path, or an empty string if the user cancels.
 * The sandbox access to the file ends when completionHandler returns, so read the file in it.
 */
void selectFileToOpen(QWindow *parentWindow, const QString &caption, const QStringList &fileExtensions, std::function<void(const QString &)> completionHandler);

} // namespace OCC::Mac::SandboxFilePicker
