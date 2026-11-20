/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>

#include <functional>

#include "macOS/fileproviderxpc.h"

#import <Foundation/Foundation.h>

#import "../../shell_integration/MacOSX/NextcloudFileProviderKit/Sources/NextcloudFileProviderXPC/include/NextcloudFileProviderXPC.h"

static_assert(__has_feature(objc_arc), "File provider XPC reachability tests require ARC.");

using namespace Qt::StringLiterals;

/// The extension side of a connection: answers the identifier request, ignores everything else.
@interface FakeClientCommunicationService : NSObject <ClientCommunicationProtocol>
@end

@implementation FakeClientCommunicationService
- (void)getFileProviderDomainIdentifierWithCompletionHandler:(void (^)(NSString *, NSError *))completionHandler
{
    completionHandler(@"domain", nil);
}
- (void)hasDirtyUserDataWithCompletionHandler:(void (^)(BOOL))completionHandler
{
    completionHandler(NO);
}
- (void)configureAccountWithUser:(NSString *)user
                          userId:(NSString *)userId
                       serverUrl:(NSString *)serverUrl
                        password:(NSString *)password
                       userAgent:(NSString *)userAgent
{
}
- (void)removeAccountConfig
{
}
- (void)deletePasswordWithCompletionHandler:(void (^)(BOOL))completionHandler
{
    completionHandler(YES);
}
- (void)setIgnoreList:(NSArray<NSString *> *)ignoreList
{
}
- (void)processFileIdsChanged:(NSArray<NSNumber *> *)fileIds completionHandler:(void (^)(BOOL))completionHandler
{
    completionHandler(YES);
}
@end

@interface FakeExtensionListenerDelegate : NSObject <NSXPCListenerDelegate>
@end

@implementation FakeExtensionListenerDelegate
- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)connection
{
    connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(ClientCommunicationProtocol)];
    connection.exportedObject = [FakeClientCommunicationService new];
    [connection resume];
    return YES;
}
@end

namespace
{

/// Records the reconnects the periodic check asks for instead of going through NSFileProviderManager.
class ReachabilityProbe : public OCC::Mac::FileProviderXPC
{
public:
    QStringList domains;
    QStringList reconnected;
    std::function<void(const QString &)> onReconnect;

    void check()
    {
        checkFileProviderDomains();
    }

    void addConnection(const QString &domain, NSXPCListenerEndpoint *endpoint)
    {
        NSXPCConnection *const connection = [[NSXPCConnection alloc] initWithListenerEndpoint:endpoint];
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(ClientCommunicationProtocol)];
        [connection resume];
        NSObject *const service = connection.remoteObjectProxy;
        _clientCommConnections.insert(domain, {(__bridge_retained void *)service, (__bridge_retained void *)connection});
    }

    [[nodiscard]] bool markedUnreachable(const QString &domain) const
    {
        return _unreachableFileProviderDomains.contains(domain);
    }

protected:
    [[nodiscard]] QStringList fileProviderDomainIdentifiers() const override
    {
        return domains;
    }

    void reconnectFileProviderDomain(const QString &domain) override
    {
        reconnected.append(domain);
        if (onReconnect) {
            onReconnect(domain);
        }
    }
};

}

class TestFileProviderXPCReachability : public QObject
{
    Q_OBJECT

    FakeExtensionListenerDelegate *_delegate = [FakeExtensionListenerDelegate new];
    NSXPCListener *_answering = [NSXPCListener anonymousListener];
    // Never resumed: a request to it gets no answer, as one to an extension process which went away.
    NSXPCListener *_gone = [NSXPCListener anonymousListener];

private Q_SLOTS:
    void initTestCase()
    {
        _answering.delegate = _delegate;
        [_answering resume];
    }

    void anAnsweringDomainIsLeftAlone()
    {
        ReachabilityProbe probe;
        probe.domains = {u"domain"_s};
        probe.addConnection(u"domain"_s, _answering.endpoint);

        probe.check();

        QVERIFY(probe.reconnected.isEmpty());
        QVERIFY(!probe.markedUnreachable(u"domain"_s));
    }

    void anUnansweringDomainIsReconnected()
    {
        ReachabilityProbe probe;
        probe.domains = {u"domain"_s};
        probe.addConnection(u"domain"_s, _gone.endpoint);

        probe.check();

        QCOMPARE(probe.reconnected, QStringList{u"domain"_s});
        QVERIFY2(probe.markedUnreachable(u"domain"_s), "The reconnect did not help, so the domain is left alone for a while.");
    }

    void aDomainWithoutConnectionIsReconnected()
    {
        ReachabilityProbe probe;
        probe.domains = {u"domain"_s};

        probe.check();

        QCOMPARE(probe.reconnected, QStringList{u"domain"_s});
    }

    void aRecentlyUnreachableDomainIsNotCheckedAgain()
    {
        ReachabilityProbe probe;
        probe.domains = {u"domain"_s};
        probe.addConnection(u"domain"_s, _gone.endpoint);
        probe.check();

        probe.check();

        QCOMPARE(probe.reconnected, QStringList{u"domain"_s});
    }

    void aReconnectWhichRepairsTheDomainCounts()
    {
        ReachabilityProbe probe;
        probe.domains = {u"domain"_s};
        probe.addConnection(u"domain"_s, _gone.endpoint);
        probe.onReconnect = [this, &probe](const QString &domain) {
            probe.addConnection(domain, _answering.endpoint);
        };

        probe.check();

        QCOMPARE(probe.reconnected, QStringList{u"domain"_s});
        QVERIFY2(!probe.markedUnreachable(u"domain"_s), "The retry reached the extension over the new connection.");
    }
};

QTEST_GUILESS_MAIN(TestFileProviderXPCReachability)
#include "testfileproviderxpcreachability.moc"
