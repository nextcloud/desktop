/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "macsandboxfolderpicker.h"

#include <QLoggingCategory>
#include <QWindow>

#include <utility>

#import <AppKit/AppKit.h>

Q_LOGGING_CATEGORY(lcMacSandboxFolderPicker, "nextcloud.gui.mac.sandbox.folderpicker", QtInfoMsg)

namespace OCC::Mac::SandboxFolderPicker {

void select(QWindow *parentWindow, const QString &caption, const QString &initialPath, std::function<void(FolderSelection)> completionHandler)
{
    qCInfo(lcMacSandboxFolderPicker) << "Opening native folder panel." << "initialPath" << initialPath;

    NSOpenPanel *const panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.canCreateDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.resolvesAliases = YES;
    panel.message = caption.toNSString();

    if (!initialPath.isEmpty()) {
        panel.directoryURL = [NSURL fileURLWithPath:initialPath.toNSString() isDirectory:YES];
    }

    // Use AppKit's asynchronous panel API instead of runModal(). Qt's Cocoa
    // event dispatcher can dismiss a nested native modal loop immediately.
    // Activating the parent first keeps the sheet on it while another app is in front.
    NSWindow *sheetParent = nil;
    if (parentWindow) {
        parentWindow->requestActivate();
        sheetParent = reinterpret_cast<NSView *>(parentWindow->winId()).window;
    }
    if (!sheetParent) {
        sheetParent = NSApp.keyWindow ?: NSApp.mainWindow;
    }
    const auto completionHandlerBlock = ^(NSModalResponse response) {
        qCInfo(lcMacSandboxFolderPicker) << "Native folder panel returned." << static_cast<NSInteger>(response);
        FolderSelection selection;
        if (response != NSModalResponseOK) {
            qCInfo(lcMacSandboxFolderPicker) << "Native folder panel was cancelled.";
            completionHandler(std::move(selection));
            return;
        }

        NSURL *const selectedURL = panel.URLs.firstObject;
        if (!selectedURL) {
            qCWarning(lcMacSandboxFolderPicker) << "Native folder panel returned no selection";
            completionHandler(std::move(selection));
            return;
        }

        qCInfo(lcMacSandboxFolderPicker) << "Native folder panel returned a selection." << QString::fromNSString(selectedURL.path);

        NSError *error = nil;
        NSData *const bookmarkData = [selectedURL bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                                           includingResourceValuesForKeys:nil
                                                            relativeToURL:nil
                                                                    error:&error];
        if (error || !bookmarkData) {
            qCWarning(lcMacSandboxFolderPicker) << "Failed to create bookmark data for native folder selection"
                                                << (error ? QString::fromNSString([error localizedDescription]) : QStringLiteral("(unknown error)"));
            [selectedURL stopAccessingSecurityScopedResource];
            completionHandler(std::move(selection));
            return;
        }

        selection.path = QString::fromNSString(selectedURL.path);
        selection.bookmarkData = QByteArray(reinterpret_cast<const char *>(bookmarkData.bytes),
                                            static_cast<int>(bookmarkData.length));

        // NSOpenPanel starts scoped access for the selected URL. Release that
        // temporary access after the bookmark has been created.
        [selectedURL stopAccessingSecurityScopedResource];

        completionHandler(std::move(selection));
    };

    if (sheetParent) {
        [panel beginSheetModalForWindow:sheetParent completionHandler:completionHandlerBlock];
    } else {
        [panel beginWithCompletionHandler:completionHandlerBlock];
    }
}

} // namespace OCC::Mac::SandboxFolderPicker
