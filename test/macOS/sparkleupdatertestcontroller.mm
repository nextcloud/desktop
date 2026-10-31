/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sparkleupdatertestcontroller.h"

#include <QtGlobal>

#import <objc/runtime.h>

namespace
{

__weak id<SPUUpdaterDelegate> delegate;
__weak SPUStandardUpdaterController *controller;
auto shouldFailInitialization = false;

}

@interface SPUStandardUpdaterController (ARCRegression)
- (instancetype)initForARCRegressionWithStartingUpdater:(BOOL)requestedStartUpdater
                                        updaterDelegate:(id<SPUUpdaterDelegate>)testUpdaterDelegate
                                     userDriverDelegate:(id<SPUStandardUserDriverDelegate>)testUserDriverDelegate;
@end

@implementation SPUStandardUpdaterController (ARCRegression)

- (instancetype)initForARCRegressionWithStartingUpdater:(BOOL)requestedStartUpdater
                                        updaterDelegate:(id<SPUUpdaterDelegate>)testUpdaterDelegate
                                     userDriverDelegate:(id<SPUStandardUserDriverDelegate>)testUserDriverDelegate
{
    Q_UNUSED(requestedStartUpdater)
    delegate = testUpdaterDelegate;
    if (shouldFailInitialization) {
        return nil;
    }
    self = [self initForARCRegressionWithStartingUpdater:NO updaterDelegate:testUpdaterDelegate userDriverDelegate:testUserDriverDelegate];
    controller = self;
    return self;
}

@end

namespace SparkleUpdaterTestController
{

void install()
{
    method_exchangeImplementations(
        class_getInstanceMethod(SPUStandardUpdaterController.class, @selector(initWithStartingUpdater:updaterDelegate:userDriverDelegate:)),
        class_getInstanceMethod(SPUStandardUpdaterController.class, @selector(initForARCRegressionWithStartingUpdater:updaterDelegate:userDriverDelegate:)));
    shouldFailInitialization = false;
    delegate = nil;
    controller = nil;
}

void restore()
{
    method_exchangeImplementations(
        class_getInstanceMethod(SPUStandardUpdaterController.class, @selector(initWithStartingUpdater:updaterDelegate:userDriverDelegate:)),
        class_getInstanceMethod(SPUStandardUpdaterController.class, @selector(initForARCRegressionWithStartingUpdater:updaterDelegate:userDriverDelegate:)));
}

void failInitialization(bool fail)
{
    shouldFailInitialization = fail;
}

id<SPUUpdaterDelegate> capturedDelegate()
{
    return delegate;
}

SPUStandardUpdaterController *capturedController()
{
    return controller;
}

}
