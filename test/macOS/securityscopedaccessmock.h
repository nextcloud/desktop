/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QStringList>
#include <QUrl>

#include <tuple>
#include <vector>

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

// Uses real URLs and simulates security-scoped access for a dedicated fixture path.
class SecurityScopedAccessMock
{
public:
    SecurityScopedAccessMock();
    ~SecurityScopedAccessMock();
    SecurityScopedAccessMock(const SecurityScopedAccessMock &) = delete;
    SecurityScopedAccessMock &operator=(const SecurityScopedAccessMock &) = delete;

    [[nodiscard]] bool install();
    [[nodiscard]] QUrl fileUrl(const QString &name = {}) const;
    [[nodiscard]] int liveUrlCount() const;

    bool granted = true;
    bool missingUrl = false;
    bool missingData = false;
    bool resolutionReturnsNil = false;
    bool resolutionError = false;
    bool stale = false;
    QByteArray bookmarkData;
    QString resolvedPath;
    QStringList startedPaths;
    QStringList stoppedPaths;
    int dataConversionCount = 0;
    int resolutionCount = 0;
    QByteArray resolvedBookmark;
    NSURLBookmarkResolutionOptions resolutionOptions = 0;
    bool relativeUrlWasNil = false;
    bool staleOutputProvided = false;
    bool errorOutputProvided = false;

private:
    bool installHook(Method method, id implementationBlock);
    [[nodiscard]] bool matchesBookmark(const void *bytes, NSUInteger length) const;

    NSString *_nativePathPrefix = nil;
    NSHashTable<NSURL *> *_urls = nil;
    std::vector<std::tuple<Method, IMP, IMP>> _hooks;
};
