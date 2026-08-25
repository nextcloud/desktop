/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QString>

class QTimer;

namespace OCC::Mac {

class FinderSyncService;

/**
 * @brief Establishes XPC communication between the app and the FinderSync extension.
 *
 * The app cannot vend a named XPC endpoint. Only a launchd job may advertise a Mach service
 * name, and a plain application is not one — `xpc_connection_create(3)` is explicit that
 * dynamic name registration is refused outside debug scenarios. Attempting it anyway is what
 * left FinderSync completely inert in 34.0.0: the listener silently failed to activate while
 * the app logged success, and the extension retried a lookup that could never resolve.
 *
 * So the endpoint is anonymous, and the FinderSyncBroker login item — which *is* a launchd
 * job — publishes it on the app's behalf. This class creates the anonymous listener, hands
 * its endpoint to the broker, and re-publishes whenever that link drops. The extension
 * collects the endpoint from the broker and connects directly, so no FinderSync traffic is
 * relayed. The extension exports FinderSyncProtocol; the app exports FinderSyncAppProtocol.
 */
class FinderSyncXPC : public QObject
{
    Q_OBJECT

public:
    explicit FinderSyncXPC(QObject *parent = nullptr);
    ~FinderSyncXPC() override;

    /**
     * @brief Create the anonymous listener and publish its endpoint to the broker.
     *
     * @param service The FinderSyncService that implements the app-side protocol.
     * @return true if the listener was created and configured. This says nothing about the
     * broker being reachable, which is inherently asynchronous — watch brokerReachable()
     * for that. It is deliberately not inferred from -[NSXPCListener resume], which returns
     * void and cannot fail.
     */
    bool startListener(Mac::FinderSyncService *service = nullptr);

    /**
     * @brief Check if we have any active FinderSync extension connections.
     * @return true if at least one extension is connected.
     */
    [[nodiscard]] bool hasActiveConnections() const;

    /**
     * @brief Whether the broker accepted our endpoint, i.e. whether the extension has any
     * way to reach us at all.
     */
    [[nodiscard]] bool isEndpointPublished() const;

Q_SIGNALS:
    /**
     * @brief Emitted whenever a new FinderSync extension establishes a connection.
     *
     * Receivers should immediately push all currently-registered sync folder paths
     * to the extension via registerPath(), because the extension has no knowledge of
     * already-active sync folders at the time it connects.
     */
    void extensionConnected();

    /**
     * @brief Emitted when the broker link comes up or goes down.
     *
     * A false value means the FinderSync extension has no route to the app: badges and the
     * context menu cannot work, whatever else is healthy. Surfacing this is the point — the
     * 34.0.0 failure was invisible precisely because nothing reported it.
     *
     * @param reachable Whether the broker accepted our endpoint.
     */
    void brokerReachableChanged(bool reachable);

public Q_SLOTS:
    /**
     * @brief Register a sync folder path with all connected FinderSync extensions.
     * @param path The absolute path to register.
     */
    void registerPath(const QString &path);

    /**
     * @brief Unregister a sync folder path from all connected FinderSync extensions.
     * @param path The absolute path to unregister.
     */
    void unregisterPath(const QString &path);

    /**
     * @brief Notify all extensions to update the view at the specified path.
     * @param path The absolute path where the view should be refreshed.
     */
    void updateViewAtPath(const QString &path);

    /**
     * @brief Send a status update for a file/folder to all connected extensions.
     * @param status The status string (e.g., "SYNC", "OK", "ERROR").
     * @param path The absolute path of the file/folder.
     */
    void setStatusResult(const QString &status, const QString &path);

    /**
     * @brief Send a localized string to all connected extensions.
     * @param key The string key.
     * @param value The localized string value.
     */
    void setLocalizedString(const QString &key, const QString &value);

    /**
     * @brief Reset menu items on all connected extensions.
     */
    void resetMenuItems();

    /**
     * @brief Add a menu item to all connected extensions.
     * @param command The command identifier.
     * @param flags The menu item flags.
     * @param text The menu item display text.
     */
    void addMenuItem(const QString &command, const QString &flags, const QString &text);

    /**
     * @brief Signal that menu items are complete.
     */
    void menuItemsComplete();

    /**
     * @brief Store an extension proxy (called by listener delegate).
     * @param connectionId Unique identifier for this connection.
     * @param proxy The retained proxy object (void* to NSObject<FinderSyncProtocol>*).
     * @param connection The NSXPCConnection object (void* to NSXPCConnection*).
     */
    void storeExtensionProxy(const QString &connectionId, void *proxy, void *connection);

    /**
     * @brief Remove an extension proxy when connection is invalidated (called by listener delegate).
     * @param connection The NSXPCConnection object (void* to NSXPCConnection*).
     */
    void removeExtensionProxy(void *connection);

private:
    //! Snapshot current proxy list under the mutex for safe iteration without holding the lock.
    [[nodiscard]] QList<void *> currentProxies() const;

    //! Connect to the broker login item and hand it our listener endpoint.
    //! Safe to call repeatedly; used both at startup and to recover after the broker restarts.
    void publishEndpointToBroker();

    //! Tear down the broker connection so the next publish starts from a clean one.
    void releaseBrokerConnection();

    //! Update the published state and emit brokerReachableChanged() on an actual change.
    void setEndpointPublished(bool published);

    /**
     * @brief How far the recovery ladder has climbed after failing to publish an endpoint.
     *
     * Bounded by attempt *counts* rather than by growing delays, so it cannot run forever: at
     * most one register and one re-register happen per healthy period, and re-registering mutates
     * Background Task Management state, which is not something to retry freely.
     */
    enum class SelfHealStage {
        Republish, //!< Just try publishing again; covers a broker that restarted.
        Register, //!< The login item may have lost its registration; register it.
        Reregister, //!< Registration looks fine but is not serving; restart the login item.
        GaveUp, //!< Reported once, then silent. Only a state change revives it.
    };

    //! Why a broker restart was asked for. Both reasons share one budget; see requestBrokerRestart().
    enum class BrokerRestartReason {
        VersionMismatch, //!< The running broker is from a previous version of the app.
        Unreachable, //!< The broker is registered but we cannot publish to it.
    };

    //! Single owner of every Background Task Management mutation, so the two reasons above cannot
    //! race into SMAppServiceErrorDomain code 1 or spend the budget twice.
    void requestBrokerRestart(BrokerRestartReason reason, const QString &brokerVersion = {});

    //! Arm the timer that drives the ladder. @p delayMs is short after an explicit failure and
    //! long when we are waiting to find out whether a publish silently went nowhere.
    void scheduleNextPublishAttempt(int delayMs);

    //! Advance the ladder one stage. GUI thread only.
    void advanceSelfHeal();

    //! Forget the ladder's progress. Called only after an endpoint has actually been published,
    //! so a healthy period earns a fresh budget and a broken one cannot loop.
    void resetSelfHeal();

    /**
     * @brief Called from the broker connection's failure paths. GUI thread only.
     *
     * @param attempt The publish attempt the reporting callback belongs to. Callbacks from a
     * superseded attempt are ignored: releasing an NSXPCConnection does not retract callbacks
     * already queued against it, so without this a failure from a dead connection would climb
     * the ladder on behalf of the healthy one that replaced it.
     */
    void handlePublishFailure(quint64 attempt, const QString &reason);

    //! Objective-C listener object (NSXPCListener*), created with +anonymousListener
    void *_listener = nullptr;

    //! Objective-C connection to the broker login item (NSXPCConnection*)
    void *_brokerConnection = nullptr;

    //! Whether the broker has acknowledged our endpoint. Written from XPC reply blocks and
    //! read from the GUI thread, so it is protected by _proxiesMutex like the proxy tables.
    bool _endpointPublished = false;

    //! Drives the recovery ladder. Single-shot; owned by this object, so the parent destroys it.
    QTimer *_publishTimer = nullptr;

    //! All of the following are GUI-thread-only. Every path that touches them arrives through
    //! QMetaObject::invokeMethod(this, …, Qt::QueuedConnection), so unlike the proxy tables above
    //! they need no mutex — but they must never be read from an XPC reply block directly.
    //! Identifies the current publish attempt, so asynchronous broker callbacks can be scoped to
    //! the connection that installed them.
    quint64 _brokerAttempt = 0;
    SelfHealStage _selfHealStage = SelfHealStage::Republish;
    int _republishAttemptsLeft = 2;
    bool _brokerRestartInFlight = false;
    bool _brokerRestartSpent = false;
    QString _restartedForBrokerVersion;

    //! Objective-C listener delegate (FinderSyncXPCListenerDelegate*)
    //! Must be retained separately because NSXPCListener holds weak reference
    void *_listenerDelegate = nullptr;

    //! Connected extension proxies, keyed by connection identifier
    //! Values are NSObject<FinderSyncProtocol>* proxies
    //! Protected by _proxiesMutex for thread-safe access
    QHash<QString, void*> _extensionProxies;

    //! Reverse mapping from NSXPCConnection* to connection identifier
    //! Used to find connectionId when connection is invalidated
    //! Protected by _proxiesMutex for thread-safe access
    QHash<void*, QString> _connectionToId;

    //! Mutex protecting _extensionProxies and _connectionToId access from multiple threads
    //! (XPC listener thread, main thread, destructor)
    mutable QMutex _proxiesMutex;
};

} // namespace OCC::Mac
