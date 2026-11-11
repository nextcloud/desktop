/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "macsandboxfilepicker.h"
#include "macsandboxpanelutils.h"

#include <QLoggingCategory>
#include <QStringList>

#include <utility>

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

static_assert(__has_feature(objc_arc), "macsandboxfilepicker requires ARC.");

Q_LOGGING_CATEGORY(lcMacSandboxFilePicker, "nextcloud.gui.mac.sandbox.filepicker", QtInfoMsg)

namespace OCC::Mac::SandboxFilePicker
{

namespace
{

NSArray<UTType *> *contentTypes(const QStringList &fileExtensions)
{
    NSMutableArray<UTType *> *const types = [NSMutableArray array];
    for (const auto &fileExtension : fileExtensions) {
        if (UTType *const type = [UTType typeWithFilenameExtension:fileExtension.toNSString()]) {
            [types addObject:type];
        }
    }
    return types;
}

void beginFilePanel(NSSavePanel *panel, QWindow *parentWindow, std::function<void(const QString &)> completionHandler)
{
    SandboxPanelUtils::beginSheet(panel, parentWindow, ^(NSModalResponse response) {
        NSURL *const url = response == NSModalResponseOK ? panel.URL : nil;
        if (!url) {
            qCInfo(lcMacSandboxFilePicker) << "Native file panel was cancelled.";
            completionHandler({});
            return;
        }

        qCInfo(lcMacSandboxFilePicker) << "Native file panel returned a selection." << QString::fromNSString(url.path);
        completionHandler(QString::fromNSString(url.path));

        // The panel starts scoped access for the selected URL. Release it once the caller is done with the file.
        [url stopAccessingSecurityScopedResource];
    });
}

} // namespace

void selectFileToSave(QWindow *parentWindow,
                      const QString &caption,
                      const QString &directory,
                      const QString &fileExtension,
                      std::function<void(const QString &)> completionHandler)
{
    qCInfo(lcMacSandboxFilePicker) << "Opening native save panel." << "directory" << directory;

    NSSavePanel *const panel = [NSSavePanel savePanel];
    panel.message = caption.toNSString();
    panel.canCreateDirectories = YES;
    panel.allowedContentTypes = contentTypes({fileExtension});

    if (!directory.isEmpty()) {
        panel.directoryURL = [NSURL fileURLWithPath:directory.toNSString() isDirectory:YES];
    }

    beginFilePanel(panel, parentWindow, std::move(completionHandler));
}

} // namespace OCC::Mac::SandboxFilePicker
