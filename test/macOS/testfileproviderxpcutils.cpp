/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>

#include "macOS/fileproviderxpc_mac_utils.h"

#import <FileProvider/FileProvider.h>

@interface TestFileProviderService : NSObject
@property (nonatomic, copy) NSString *name;
@end

@implementation TestFileProviderService
@end

@interface TestFileProviderManager : NSObject
@property (nonatomic, retain) NSFileProviderService *service;
@end

@implementation TestFileProviderManager

- (void)getServiceWithName:(NSFileProviderServiceName)serviceName
             itemIdentifier:(NSFileProviderItemIdentifier)itemIdentifier
          completionHandler:(void (^)(NSFileProviderService *service, NSError *error))completionHandler
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        completionHandler(self.service, nil);
    });
}

@end

class TestFileProviderXPCUtils : public QObject
{
    Q_OBJECT

private slots:
    void concurrentServiceLookupsCanBeCollected()
    {
        constexpr auto managerCount = 128;
        NSMutableArray<NSFileProviderManager *> * const managers = NSMutableArray.array;

        for (auto index = 0; index < managerCount; ++index) {
            const auto service = [TestFileProviderService new];
            service.name = [NSString stringWithFormat:@"service-%d", index];

            const auto manager = [TestFileProviderManager new];
            manager.service = (NSFileProviderService *)service;
            [managers addObject:(NSFileProviderManager *)manager];
            [service release];
            [manager release];
        }

        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(managers);
        QCOMPARE(services.count, managerCount);
    }

    void serviceWithoutNameIsIgnored()
    {
        const auto service = [TestFileProviderService new];
        const auto manager = [TestFileProviderManager new];
        manager.service = (NSFileProviderService *)service;

        const auto services = OCC::Mac::FileProviderXPCUtils::getFileProviderServices(@[(NSFileProviderManager *)manager]);
        QCOMPARE(services.count, 0);

        [service release];
        [manager release];
    }
};

QTEST_APPLESS_MAIN(TestFileProviderXPCUtils)
#include "testfileproviderxpcutils.moc"
