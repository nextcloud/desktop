/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cocoainitializer.h"

#include <QtTest>

#include <stdexcept>

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

static_assert(__has_feature(objc_arc), "CocoaInitializer tests require ARC.");

class TestCocoaInitializerMacOS : public QObject
{
    Q_OBJECT

    using BeginActivity = id<NSObject> (*)(id, SEL, NSActivityOptions, NSString *);
    using EndActivity = void (*)(id, SEL, id<NSObject>);

    Method beginActivityMethod = nullptr;
    Method endActivityMethod = nullptr;
    IMP originalBeginActivity = nullptr;
    IMP originalEndActivity = nullptr;
    IMP mockedBeginActivity = nullptr;
    IMP mockedEndActivity = nullptr;
    __weak id activityToken = nil;
    NSActivityOptions activityOptions = 0;
    int beginCount = 0;
    int endCount = 0;
    bool endedWithExpectedToken = false;
    bool endedOnMainThread = false;

private Q_SLOTS:
    void init()
    {
        beginCount = 0;
        endCount = 0;
        activityOptions = 0;
        activityToken = nil;
        endedWithExpectedToken = false;
        endedOnMainThread = false;
        const auto processInfoClass = object_getClass(NSProcessInfo.processInfo);
        beginActivityMethod = class_getInstanceMethod(processInfoClass, @selector(beginActivityWithOptions:reason:));
        endActivityMethod = class_getInstanceMethod(processInfoClass, @selector(endActivity:));
        originalBeginActivity = method_getImplementation(beginActivityMethod);
        originalEndActivity = method_getImplementation(endActivityMethod);
        const auto beginActivity = reinterpret_cast<BeginActivity>(originalBeginActivity);
        const auto endActivity = reinterpret_cast<EndActivity>(originalEndActivity);
        mockedBeginActivity = imp_implementationWithBlock(^id<NSObject>(id receiver, NSActivityOptions options, NSString *reason) {
            if ([reason isEqualToString:@"Keep syncing while running in the background"]) {
                ++beginCount;
                activityOptions = options;
                const auto token = [NSObject new];
                activityToken = token;
                return token;
            }
            return beginActivity(receiver, @selector(beginActivityWithOptions:reason:), options, reason);
        });
        mockedEndActivity = imp_implementationWithBlock(^(id receiver, id<NSObject> token) {
            if (token == activityToken) {
                ++endCount;
                endedWithExpectedToken = true;
                endedOnMainThread = NSThread.isMainThread;
                return;
            }
            endActivity(receiver, @selector(endActivity:), token);
        });
        method_setImplementation(beginActivityMethod, mockedBeginActivity);
        method_setImplementation(endActivityMethod, mockedEndActivity);
    }

    void cleanup()
    {
        method_setImplementation(beginActivityMethod, originalBeginActivity);
        method_setImplementation(endActivityMethod, originalEndActivity);
        imp_removeBlock(mockedBeginActivity);
        imp_removeBlock(mockedEndActivity);
    }

    void runPreservesMainThreadAndNativeResourceLifetime()
    {
        constexpr auto expectedExitCode = 42;
        __weak NSObject *pooledObject = nil;
        auto callbackOnMainThread = false;
        auto activityAliveDuringCallback = false;
        auto objectAliveDuringCallback = false;
        @autoreleasepool {
            const auto callingThread = NSThread.currentThread;
            const auto result = OCC::Mac::CocoaInitializer::run([&] {
                callbackOnMainThread = NSThread.isMainThread && NSThread.currentThread == callingThread;
                activityAliveDuringCallback = activityToken != nil && beginCount == 1 && endCount == 0;
                __autoreleasing id object = [NSObject new];
                pooledObject = object;
                object = nil;
                objectAliveDuringCallback = pooledObject != nil;
                return expectedExitCode;
            });
            QCOMPARE(result, expectedExitCode);
            QVERIFY(callbackOnMainThread);
            QVERIFY(activityAliveDuringCallback);
            QVERIFY(objectAliveDuringCallback);
            QVERIFY(pooledObject == nil);
            QVERIFY(activityToken == nil);
            QCOMPARE(beginCount, 1);
            QCOMPARE(endCount, 1);
            QCOMPARE(activityOptions, NSActivityBackground);
            QVERIFY(endedWithExpectedToken);
            QVERIFY(endedOnMainThread);
        }
    }

    void runCleansUpBeforeRethrowing_data()
    {
        QTest::addColumn<bool>("nativeException");
        QTest::newRow("C++ exception") << false;
        QTest::newRow("Objective-C exception") << true;
    }

    void runCleansUpBeforeRethrowing()
    {
        QFETCH(bool, nativeException);
        __weak NSObject *pooledObject = nil;
        auto caughtExpectedException = false;
        auto objectAliveDuringCallback = false;
        @autoreleasepool {
            try {
                @try {
                    OCC::Mac::CocoaInitializer::run([&]() -> int {
                        __autoreleasing id object = [NSObject new];
                        pooledObject = object;
                        object = nil;
                        objectAliveDuringCallback = pooledObject != nil;
                        if (nativeException) {
                            @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"Cocoa initializer callback" userInfo:nil];
                        }
                        throw std::runtime_error("Cocoa initializer callback");
                    });
                } @catch (NSException *exception) {
                    caughtExpectedException = nativeException && [exception.name isEqualToString:NSInvalidArgumentException];
                }
            } catch (const std::runtime_error &) {
                caughtExpectedException = !nativeException;
            }
            QVERIFY(caughtExpectedException);
            QVERIFY(objectAliveDuringCallback);
            QVERIFY(pooledObject == nil);
            QVERIFY(activityToken == nil);
            QCOMPARE(beginCount, 1);
            QCOMPARE(endCount, 1);
            QVERIFY(endedWithExpectedToken);
            QVERIFY(endedOnMainThread);
        }
    }
};

QTEST_APPLESS_MAIN(TestCocoaInitializerMacOS)
#include "testcocoainitializermacos.moc"
