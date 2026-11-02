/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "localnetworkpermissiontestconnection.h"

#include <QtTest>

using namespace Qt::StringLiterals;

namespace LocalNetworkPermissionMock
{

__weak NSObject *endpointObject;
__weak NSObject *parametersObject;
__weak NSObject *pathObject;
__weak LocalNetworkPermissionTestConnection *connectionObject;
dispatch_block_t timeoutHandler;
auto failEndpoint = false;
auto failParameters = false;
auto failConnection = false;
auto hasPath = true;
auto deniedPath = false;
auto cancelCount = 0;

nw_endpoint_t createEndpoint(const char *, const char *) NS_RETURNS_RETAINED;
nw_endpoint_t createEndpoint(const char *, const char *)
{
    if (failEndpoint) {
        return nil;
    }
    const auto endpoint = [NSObject new];
    endpointObject = endpoint;
    return (nw_endpoint_t)endpoint;
}

nw_parameters_t createParameters(nw_parameters_configure_protocol_block_t, nw_parameters_configure_protocol_block_t) NS_RETURNS_RETAINED;
nw_parameters_t createParameters(nw_parameters_configure_protocol_block_t, nw_parameters_configure_protocol_block_t)
{
    if (failParameters) {
        return nil;
    }
    const auto parameters = [NSObject new];
    parametersObject = parameters;
    return (nw_parameters_t)parameters;
}

void setPreferNoProxy(nw_parameters_t, bool)
{
}

nw_connection_t createConnection(nw_endpoint_t, nw_parameters_t) NS_RETURNS_RETAINED;
nw_connection_t createConnection(nw_endpoint_t, nw_parameters_t)
{
    if (failConnection) {
        return nil;
    }
    const auto connection = [LocalNetworkPermissionTestConnection new];
    connectionObject = connection;
    return (nw_connection_t)connection;
}

void setQueue(nw_connection_t, dispatch_queue_t)
{
}

void setPathHandler(nw_connection_t connection, nw_connection_path_event_handler_t handler)
{
    ((LocalNetworkPermissionTestConnection *)connection).pathHandler = handler;
}

void setStateHandler(nw_connection_t connection, nw_connection_state_changed_handler_t handler)
{
    ((LocalNetworkPermissionTestConnection *)connection).stateHandler = handler;
}

void startConnection(nw_connection_t connection)
{
    ((LocalNetworkPermissionTestConnection *)connection).started = YES;
}

void cancelConnection(nw_connection_t)
{
    ++cancelCount;
}

nw_path_t copyPath(nw_connection_t) NS_RETURNS_RETAINED;
nw_path_t copyPath(nw_connection_t)
{
    if (!hasPath) {
        return nil;
    }
    const auto path = [NSObject new];
    pathObject = path;
    return (nw_path_t)path;
}

nw_path_status_t pathStatus(nw_path_t)
{
    return deniedPath ? nw_path_status_unsatisfied : nw_path_status_satisfied;
}

nw_path_unsatisfied_reason_t pathReason(nw_path_t)
{
    return nw_path_unsatisfied_reason_local_network_denied;
}

void scheduleTimeout(dispatch_time_t, dispatch_queue_t, dispatch_block_t handler)
{
    timeoutHandler = handler;
}

} // namespace LocalNetworkPermissionMock

// Compile the real implementation with deterministic native entry points. No connection is started.
#define nw_endpoint_create_host LocalNetworkPermissionMock::createEndpoint
#define nw_parameters_create_secure_tcp LocalNetworkPermissionMock::createParameters
#define nw_parameters_set_prefer_no_proxy LocalNetworkPermissionMock::setPreferNoProxy
#define nw_connection_create LocalNetworkPermissionMock::createConnection
#define nw_connection_set_queue LocalNetworkPermissionMock::setQueue
#define nw_connection_set_path_changed_handler LocalNetworkPermissionMock::setPathHandler
#define nw_connection_set_state_changed_handler LocalNetworkPermissionMock::setStateHandler
#define nw_connection_start LocalNetworkPermissionMock::startConnection
#define nw_connection_cancel LocalNetworkPermissionMock::cancelConnection
#define nw_connection_copy_current_path LocalNetworkPermissionMock::copyPath
#define nw_path_get_status LocalNetworkPermissionMock::pathStatus
#define nw_path_get_unsatisfied_reason LocalNetworkPermissionMock::pathReason
#define dispatch_after LocalNetworkPermissionMock::scheduleTimeout
#include "../../src/gui/macOS/localnetworkpermission.mm"
#undef nw_endpoint_create_host
#undef nw_parameters_create_secure_tcp
#undef nw_parameters_set_prefer_no_proxy
#undef nw_connection_create
#undef nw_connection_set_queue
#undef nw_connection_set_path_changed_handler
#undef nw_connection_set_state_changed_handler
#undef nw_connection_start
#undef nw_connection_cancel
#undef nw_connection_copy_current_path
#undef nw_path_get_status
#undef nw_path_get_unsatisfied_reason
#undef dispatch_after

class TestLocalNetworkPermissionMacOS : public QObject
{
    Q_OBJECT

    auto callback()
    {
        return [this](bool denied) {
            ++callbackCount;
            lastDenied = denied;
        };
    }

    int callbackCount = 0;
    bool lastDenied = false;

private Q_SLOTS:
    void init()
    {
        if (@available(macOS 15.0, *)) {
        } else {
            QSKIP("Native permission probes require macOS 15 or newer.");
        }
        callbackCount = 0;
        lastDenied = false;
        LocalNetworkPermissionMock::failEndpoint = false;
        LocalNetworkPermissionMock::failParameters = false;
        LocalNetworkPermissionMock::failConnection = false;
        LocalNetworkPermissionMock::hasPath = true;
        LocalNetworkPermissionMock::deniedPath = false;
        LocalNetworkPermissionMock::cancelCount = 0;
    }

    void cleanup()
    {
        if (LocalNetworkPermissionMock::timeoutHandler != nil) {
            LocalNetworkPermissionMock::timeoutHandler();
        }
        LocalNetworkPermissionMock::timeoutHandler = nil;
        QCoreApplication::processEvents();
    }

    void invalidUrlCompletesAsynchronously()
    {
        OCC::LocalNetworkPermission::checkDeniedForConnection({}, this, callback());
        QCOMPARE(callbackCount, 0);
        QTRY_COMPARE(callbackCount, 1);
        QVERIFY(!lastDenied);
        QVERIFY(LocalNetworkPermissionMock::connectionObject == nil);
    }

    void allocationFailuresReleaseNativeObjects_data()
    {
        QTest::addColumn<int>("failure");
        QTest::newRow("endpoint") << 0;
        QTest::newRow("parameters") << 1;
        QTest::newRow("connection") << 2;
    }

    void allocationFailuresReleaseNativeObjects()
    {
        QFETCH(int, failure);
        LocalNetworkPermissionMock::failEndpoint = failure == 0;
        LocalNetworkPermissionMock::failParameters = failure == 1;
        LocalNetworkPermissionMock::failConnection = failure == 2;
        @autoreleasepool {
            OCC::LocalNetworkPermission::checkDeniedForConnection(QUrl("https://example.test"_L1), this, callback());
        }
        QTRY_COMPARE(callbackCount, 1);
        QVERIFY(!lastDenied);
        QVERIFY(LocalNetworkPermissionMock::endpointObject == nil);
        QVERIFY(LocalNetworkPermissionMock::parametersObject == nil);
        QCOMPARE(LocalNetworkPermissionMock::cancelCount, 0);
    }

    void completionReleasesConnectionAndIgnoresLateCallbacks_data()
    {
        QTest::addColumn<int>("event");
        QTest::addColumn<bool>("denied");
        QTest::newRow("ready") << 0 << false;
        QTest::newRow("waiting denied") << 1 << true;
        QTest::newRow("failed denied") << 2 << true;
        QTest::newRow("path denied") << 3 << true;
        QTest::newRow("timeout denied") << 4 << true;
        QTest::newRow("timeout allowed") << 4 << false;
    }

    void completionReleasesConnectionAndIgnoresLateCallbacks()
    {
        QFETCH(int, event);
        QFETCH(bool, denied);
        LocalNetworkPermissionMock::deniedPath = denied;
        @autoreleasepool {
            OCC::LocalNetworkPermission::checkDeniedForConnection(QUrl("https://example.test"_L1), this, callback());
        }
        QVERIFY(LocalNetworkPermissionMock::connectionObject.started);
        const auto stateHandler = LocalNetworkPermissionMock::connectionObject.stateHandler;
        const auto pathHandler = LocalNetworkPermissionMock::connectionObject.pathHandler;
        switch (event) {
        case 0:
            stateHandler(nw_connection_state_ready, nil);
            break;
        case 1:
            stateHandler(nw_connection_state_waiting, nil);
            break;
        case 2:
            stateHandler(nw_connection_state_failed, nil);
            break;
        case 3:
            pathHandler((nw_path_t)[NSObject new]);
            break;
        default:
            LocalNetworkPermissionMock::timeoutHandler();
            break;
        }
        QTRY_COMPARE(callbackCount, 1);
        QCOMPARE(lastDenied, denied);
        QCOMPARE(LocalNetworkPermissionMock::cancelCount, 1);
        QVERIFY(LocalNetworkPermissionMock::connectionObject == nil);
        QVERIFY(LocalNetworkPermissionMock::pathObject == nil);
        stateHandler(nw_connection_state_ready, nil);
        stateHandler(nw_connection_state_failed, nil);
        pathHandler((nw_path_t)[NSObject new]);
        LocalNetworkPermissionMock::timeoutHandler();
        QCoreApplication::processEvents();
        QCOMPARE(callbackCount, 1);
        QCOMPARE(LocalNetworkPermissionMock::cancelCount, 1);
    }

    void unresolvedPathWaitsForTimeout_data()
    {
        QTest::addColumn<bool>("hasPath");
        QTest::newRow("missing path") << false;
        QTest::newRow("allowed path") << true;
    }

    void unresolvedPathWaitsForTimeout()
    {
        QFETCH(bool, hasPath);
        LocalNetworkPermissionMock::hasPath = hasPath;
        OCC::LocalNetworkPermission::checkDeniedForConnection(QUrl("https://example.test"_L1), this, callback());
        LocalNetworkPermissionMock::connectionObject.stateHandler(nw_connection_state_waiting, nil);
        QCOMPARE(callbackCount, 0);
        QCOMPARE(LocalNetworkPermissionMock::cancelCount, 0);
        LocalNetworkPermissionMock::timeoutHandler();
        QTRY_COMPARE(callbackCount, 1);
        QVERIFY(!lastDenied);
        QCOMPARE(LocalNetworkPermissionMock::cancelCount, 1);
        QVERIFY(LocalNetworkPermissionMock::connectionObject == nil);
    }

    void destroyedContextSuppressesCompletion()
    {
        const auto context = new QObject;
        OCC::LocalNetworkPermission::checkDeniedForConnection(QUrl("https://example.test"_L1), context, callback());
        delete context;
        LocalNetworkPermissionMock::connectionObject.stateHandler(nw_connection_state_ready, nil);
        QCoreApplication::processEvents();
        QCOMPARE(callbackCount, 0);
        QCOMPARE(LocalNetworkPermissionMock::cancelCount, 1);
        QVERIFY(LocalNetworkPermissionMock::connectionObject == nil);
    }

    void destroyedContextSuppressesQueuedCallback()
    {
        const auto context = new QObject;
        OCC::LocalNetworkPermission::checkDeniedForConnection({}, context, callback());
        delete context;
        QCoreApplication::processEvents();
        QCOMPARE(callbackCount, 0);
    }
};

QTEST_GUILESS_MAIN(TestLocalNetworkPermissionMacOS)
#include "testlocalnetworkpermissionmacos.moc"
