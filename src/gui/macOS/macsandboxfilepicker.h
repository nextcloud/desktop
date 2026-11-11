/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

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

} // namespace OCC::Mac::SandboxFilePicker
