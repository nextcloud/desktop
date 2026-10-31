/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "securityscopedaccessmock.h"

#include <cstring>
#include <ranges>

static_assert(__has_feature(objc_arc), "The native security-scoped access fixture requires ARC.");

using namespace Qt::StringLiterals;

SecurityScopedAccessMock::SecurityScopedAccessMock()
    : bookmarkData("nextcloud-bookmark\0fixture"_ba)
    , resolvedPath(fileUrl().toLocalFile())
{
}

SecurityScopedAccessMock::~SecurityScopedAccessMock()
{
    @autoreleasepool {
        for (const auto &[method, original, replacement] : std::views::reverse(_hooks)) {
            method_setImplementation(method, original);
            imp_removeBlock(replacement);
        }
        _urls = nil;
        _nativePathPrefix = nil;
    }
}

QUrl SecurityScopedAccessMock::fileUrl(const QString &name) const
{
    return QUrl::fromLocalFile(u"/nextcloud-arc-security-scope/"_s + (name.isEmpty() ? u"resource"_s : name));
}

int SecurityScopedAccessMock::liveUrlCount() const
{
    @autoreleasepool {
        return static_cast<int>(_urls.allObjects.count);
    }
}

bool SecurityScopedAccessMock::matchesBookmark(const void *bytes, NSUInteger length) const
{
    return !bookmarkData.isEmpty() && bytes && length == static_cast<NSUInteger>(bookmarkData.size())
        && std::memcmp(bytes, bookmarkData.constData(), length) == 0;
}

bool SecurityScopedAccessMock::installHook(Method method, id implementationBlock)
{
    if (!method) {
        return false;
    }
    const auto replacement = imp_implementationWithBlock(implementationBlock);
    if (!replacement) {
        return false;
    }
    _hooks.emplace_back(method, method_getImplementation(method), replacement);
    method_setImplementation(method, replacement);
    return true;
}

bool SecurityScopedAccessMock::install()
{
    @autoreleasepool {
        _nativePathPrefix = @"/nextcloud-arc-security-scope/";
        _urls = [NSHashTable weakObjectsHashTable];
        const auto urlClass = object_getClass([NSURL fileURLWithPath:_nativePathPrefix]);

        const auto factoryMethod = class_getClassMethod(NSURL.class, @selector(fileURLWithPath:));
        if (!factoryMethod) {
            return false;
        }
        const auto factoryOriginal = reinterpret_cast<NSURL *(*)(id, SEL, NSString *)>(method_getImplementation(factoryMethod));
        if (!installHook(factoryMethod, ^NSURL *(id receiver, NSString *path) {
                const auto matchesPath = [path hasPrefix:_nativePathPrefix];
                if (missingUrl && matchesPath) {
                    return nil;
                }
                const auto url = factoryOriginal(receiver, @selector(fileURLWithPath:), path);
                if (url && matchesPath) {
                    [_urls addObject:url];
                }
                return url;
            })) {
            return false;
        }

        const auto startMethod = class_getInstanceMethod(urlClass, @selector(startAccessingSecurityScopedResource));
        if (!startMethod) {
            return false;
        }
        const auto startOriginal = reinterpret_cast<BOOL (*)(id, SEL)>(method_getImplementation(startMethod));
        if (!installHook(startMethod, ^BOOL(NSURL *url) {
                if (![url.path hasPrefix:_nativePathPrefix]) {
                    return startOriginal(url, @selector(startAccessingSecurityScopedResource));
                }
                startedPaths.append(QString::fromNSString(url.path));
                return granted;
            })) {
            return false;
        }

        const auto stopMethod = class_getInstanceMethod(urlClass, @selector(stopAccessingSecurityScopedResource));
        if (!stopMethod) {
            return false;
        }
        const auto stopOriginal = reinterpret_cast<void (*)(id, SEL)>(method_getImplementation(stopMethod));
        if (!installHook(stopMethod, ^(NSURL *url) {
                if (![url.path hasPrefix:_nativePathPrefix]) {
                    stopOriginal(url, @selector(stopAccessingSecurityScopedResource));
                    return;
                }
                stoppedPaths.append(QString::fromNSString(url.path));
            })) {
            return false;
        }

        const auto dataMethod = class_getClassMethod(NSData.class, @selector(dataWithBytes:length:));
        if (!dataMethod) {
            return false;
        }
        const auto dataOriginal = reinterpret_cast<NSData *(*)(id, SEL, const void *, NSUInteger)>(method_getImplementation(dataMethod));
        if (!installHook(dataMethod, ^NSData *(id receiver, const void *bytes, NSUInteger length) {
                if (matchesBookmark(bytes, length)) {
                    ++dataConversionCount;
                    if (missingData) {
                        return nil;
                    }
                }
                return dataOriginal(receiver, @selector(dataWithBytes:length:), bytes, length);
            })) {
            return false;
        }

        const auto resolverSelector = @selector(URLByResolvingBookmarkData:options:relativeToURL:bookmarkDataIsStale:error:);
        const auto resolverMethod = class_getClassMethod(NSURL.class, resolverSelector);
        if (!resolverMethod) {
            return false;
        }
        const auto resolverOriginal = reinterpret_cast<NSURL *(*)(id, SEL, NSData *, NSURLBookmarkResolutionOptions, NSURL *, BOOL *, NSError **)>(
            method_getImplementation(resolverMethod));
        return installHook(resolverMethod,
                           ^NSURL *(id receiver, NSData *data, NSURLBookmarkResolutionOptions options, NSURL *relativeUrl, BOOL *isStale, NSError **error) {
                               if (!matchesBookmark(data.bytes, data.length)) {
                                   return resolverOriginal(receiver, resolverSelector, data, options, relativeUrl, isStale, error);
                               }
                               ++resolutionCount;
                               resolvedBookmark = QByteArray(static_cast<const char *>(data.bytes), static_cast<qsizetype>(data.length));
                               resolutionOptions = options;
                               relativeUrlWasNil = relativeUrl == nil;
                               staleOutputProvided = isStale != nullptr;
                               errorOutputProvided = error != nullptr;
                               if (isStale) {
                                   *isStale = stale;
                               }
                               if (error) {
                                   *error = resolutionError ? [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadCorruptFileError userInfo:nil] : nil;
                               }
                               return resolutionReturnsNil ? nil : [NSURL fileURLWithPath:resolvedPath.toNSString()];
                           });
    }
}
