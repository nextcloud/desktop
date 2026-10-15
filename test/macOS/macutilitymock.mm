/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "macutilitymock.h"

#include <ranges>
#include <tuple>
#include <vector>

#import <objc/runtime.h>

static_assert(__has_feature(objc_arc), "The native macOS utility fixture requires ARC.");

@implementation MacUtilityMock {
    __weak MacUtilityMock *_controller;
    NSHashTable<MacUtilityMock *> *_services;
    NSHashTable<NSError *> *_errors;
    NSHashTable<NSString *> *_styles;
    std::vector<std::tuple<Method, IMP, IMP>> _hooks;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _operationSucceeds = YES;
        _includesError = YES;
        _services = [NSHashTable weakObjectsHashTable];
        _errors = [NSHashTable weakObjectsHashTable];
        _styles = [NSHashTable weakObjectsHashTable];
    }
    return self;
}

- (instancetype)initWithController:(MacUtilityMock *)controller
{
    self = [super init];
    if (self) {
        _controller = controller;
    }
    return self;
}

- (void)dealloc
{
    [self uninstall];
}

- (void)uninstall
{
    for (const auto &[method, original, replacement] : std::views::reverse(_hooks)) {
        method_setImplementation(method, original);
        imp_removeBlock(replacement);
    }
    _hooks.clear();
}

- (BOOL)installHook:(Method)method block:(id)implementationBlock
{
    if (!method) {
        return NO;
    }
    const auto replacement = imp_implementationWithBlock(implementationBlock);
    if (!replacement) {
        return NO;
    }
    _hooks.emplace_back(method, method_getImplementation(method), replacement);
    method_setImplementation(method, replacement);
    return YES;
}

- (BOOL)install
{
    __weak MacUtilityMock *weakSelf = self;
    if (![self installHook:class_getClassMethod(SMAppService.class, @selector(mainAppService))
                     block:^SMAppService *(id) {
                         const auto controller = weakSelf;
                         if (!controller) {
                             return nil;
                         }
                         ++controller->_factoryCount;
                         const auto service = [[MacUtilityMock alloc] initWithController:controller];
                         [controller->_services addObject:service];
                         return (SMAppService *)service;
                     }]) {
        return NO;
    }

    const auto preferenceMethod = class_getInstanceMethod(NSUserDefaults.class, @selector(stringForKey:));
    if (!preferenceMethod) {
        return NO;
    }
    const auto original = reinterpret_cast<NSString *(*)(id, SEL, NSString *)>(method_getImplementation(preferenceMethod));
    return [self installHook:preferenceMethod
                       block:^NSString *(NSUserDefaults *defaults, NSString *key) {
                           const auto controller = weakSelf;
                           if (!controller || defaults != NSUserDefaults.standardUserDefaults || ![key isEqualToString:@"AppleInterfaceStyle"]) {
                               return original(defaults, @selector(stringForKey:), key);
                           }
                           ++controller->_preferenceReadCount;
                           if (!controller.style) {
                               return nil;
                           }
                           const auto style = [NSMutableString stringWithString:controller.style];
                           [controller->_styles addObject:style];
                           return style;
                       }];
}

- (SMAppServiceStatus)status
{
    const auto controller = _controller;
    if (!controller) {
        return SMAppServiceStatusNotRegistered;
    }
    ++controller->_statusReadCount;
    return controller.configuredStatus;
}

- (BOOL)performOperation:(NSError *__autoreleasing *)error
{
    const auto controller = _controller;
    if (!controller) {
        return NO;
    }
    controller->_errorOutputProvided = error != nullptr;
    if (!controller.operationSucceeds) {
        if (error) {
            *error = controller.includesError ? [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteNoPermissionError userInfo:nil] : nil;
            if (*error) {
                [controller->_errors addObject:*error];
            }
        }
        return NO;
    }
    controller.configuredStatus = controller.updatedStatus;
    if (error) {
        *error = nil;
    }
    return YES;
}

- (BOOL)registerAndReturnError:(NSError *__autoreleasing *)error
{
    const auto controller = _controller;
    if (controller) {
        ++controller->_registerCount;
    }
    return [self performOperation:error];
}

- (BOOL)unregisterAndReturnError:(NSError *__autoreleasing *)error
{
    const auto controller = _controller;
    if (controller) {
        ++controller->_unregisterCount;
    }
    return [self performOperation:error];
}

- (NSInteger)liveServiceCount
{
    @autoreleasepool {
        return _services.allObjects.count;
    }
}

- (NSInteger)liveErrorCount
{
    @autoreleasepool {
        return _errors.allObjects.count;
    }
}

- (NSInteger)liveStyleCount
{
    @autoreleasepool {
        return _styles.allObjects.count;
    }
}

@end
