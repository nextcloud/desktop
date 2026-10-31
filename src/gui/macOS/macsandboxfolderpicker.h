/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

class QWindow;

namespace OCC::Mac::SandboxFolderPicker
{

struct FolderSelection {
    QString path;
    QByteArray bookmarkData;
};

/**
 * Select a local folder with the native macOS open panel.
 *
 * The bookmark is created from the URL returned by NSOpenPanel before its
 * security scope can be lost by converting it to a path. The panel is shown as a
 * sheet of parentWindow, which is activated first; without one it falls back to
 * the key or main window.
 */
void select(QWindow *parentWindow, const QString &caption, const QString &initialPath, std::function<void(FolderSelection)> completionHandler);

} // namespace OCC::Mac::SandboxFolderPicker
