/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDBusConnection>
#include <QDBusMetaType>
#include <QProcess>
#include <QProcessEnvironment>
#include <QThread>
#include <QtTest>

#include <algorithm>

#include "common/utility.h"
#include "config.h"
#include "fakesystemdmanager.h"

using namespace Qt::StringLiterals;

class TestLaunchOnStartup : public QObject
{
    Q_OBJECT

    static constexpr auto maxCommandDurationMs = 5000;
    static constexpr auto busStartupTimeoutMs = 3000;

    QProcess _bus;
    QThread _managerThread;
    FakeSystemdManager *_manager = nullptr;
    QByteArray _originalBusAddress;
    std::unique_ptr<QTemporaryDir> _busDirectory;
    std::unique_ptr<QTemporaryDir> _directory;
    QMap<QByteArray, QByteArray> _environment;

    void configureManager(const auto &configure) const
    {
        QMetaObject::invokeMethod(
            _manager,
            [&] {
                configure(*_manager);
            },
            Qt::BlockingQueuedConnection);
    }

    [[nodiscard]] auto managerValue(const auto &read) const
    {
        auto result = decltype(read(*_manager)){};
        configureManager([&](const auto &manager) {
            result = read(manager);
        });
        return result;
    }

    [[nodiscard]] QString desktopPath(const QString &name = QString::fromLatin1(APPLICATION_SHORTNAME)) const
    {
        return _directory->filePath(u"config/autostart/"_s + name + u".desktop"_s);
    }

    [[nodiscard]] bool writeFile(const QString &path, const QByteArray &contents = {}) const
    {
        auto file = QFile(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }

    [[nodiscard]] QByteArray readFile(const QString &path) const
    {
        auto file = QFile(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }

    [[nodiscard]] QList<QDBusMessage> requests() const
    {
        return managerValue([](const auto &manager) {
            return manager.requests;
        });
    }

    [[nodiscard]] QString state() const
    {
        return managerValue([](const auto &manager) {
            return manager.state;
        });
    }

    void verifyRequests() const
    {
        const auto service = QString::fromLatin1(LINUX_APPLICATION_ID) + u".service"_s;
        for (const auto &request : requests()) {
            QCOMPARE(request.path(), u"/org/freedesktop/systemd1"_s);
            QCOMPARE(request.interface(), u"org.freedesktop.systemd1.Manager"_s);
            if (request.member() == u"GetUnitFileState") {
                QCOMPARE(request.signature(), u"s"_s);
                QCOMPARE(request.arguments(), QVariantList{service});
            } else if (request.member() == u"MaskUnitFiles") {
                QCOMPARE(request.signature(), u"asbb"_s);
                const auto arguments = request.arguments();
                QCOMPARE(qdbus_cast<QStringList>(arguments.at(0)), QStringList{service});
                QCOMPARE(arguments.at(1).toBool(), false);
                QCOMPARE(arguments.at(2).toBool(), false);
            } else {
                QCOMPARE(request.member(), u"Reload"_s);
                QVERIFY(request.signature().isEmpty());
                QVERIFY(request.arguments().isEmpty());
            }
        }
    }

    [[nodiscard]] qsizetype methodCount(const QString &method) const
    {
        const auto messages = requests();
        return std::count_if(messages.cbegin(), messages.cend(), [&](const auto &message) {
            return message.member() == method;
        });
    }

    void toggle(const bool enabled, const QString &name = QString::fromLatin1(APPLICATION_SHORTNAME)) const
    {
        OCC::Utility::setLaunchOnStartup(name, name, enabled);
    }

private Q_SLOTS:
    void initTestCase()
    {
        _originalBusAddress = qgetenv("DBUS_SESSION_BUS_ADDRESS");
        const auto daemon = QStandardPaths::findExecutable(u"dbus-daemon"_s);
        QVERIFY2(!daemon.isEmpty(), "dbus-daemon is required for the isolated systemd D-Bus tests");
        _busDirectory = std::make_unique<QTemporaryDir>();
        QVERIFY(_busDirectory->isValid());
        const auto config = _busDirectory->filePath(u"bus.conf"_s);
        QVERIFY(writeFile(config, R"(<busconfig>
<type>session</type><listen>unix:tmpdir=/tmp</listen><auth>EXTERNAL</auth>
<policy context="default"><allow send_destination="*"/><allow receive_sender="*"/><allow own="*"/></policy>
</busconfig>)"));
        _bus.start(daemon, {u"--config-file="_s + config, u"--nofork"_s, u"--nopidfile"_s, u"--print-address=1"_s});
        QVERIFY(_bus.waitForStarted(busStartupTimeoutMs));
        QVERIFY(_bus.waitForReadyRead(busStartupTimeoutMs));
        const auto address = _bus.readLine().trimmed();
        QVERIFY(!address.isEmpty());
        qputenv("DBUS_SESSION_BUS_ADDRESS", address);
        QVERIFY(QDBusConnection::sessionBus().isConnected());

        _manager = new FakeSystemdManager;
        _manager->moveToThread(&_managerThread);
        connect(&_managerThread, &QThread::finished, _manager, &QObject::deleteLater);
        _managerThread.start();
        auto started = false;
        configureManager([&](auto &manager) {
            started = manager.start(QString::fromUtf8(address));
        });
        QVERIFY(started);
    }

    void cleanupTestCase()
    {
        if (_manager) {
            configureManager([](auto &manager) {
                manager.stop();
            });
            _managerThread.quit();
            QVERIFY(_managerThread.wait(busStartupTimeoutMs));
            _manager = nullptr;
        }
        _bus.terminate();
        if (!_bus.waitForFinished(busStartupTimeoutMs)) {
            _bus.kill();
            _bus.waitForFinished(busStartupTimeoutMs);
        }
        if (_originalBusAddress.isNull()) {
            qunsetenv("DBUS_SESSION_BUS_ADDRESS");
        } else {
            qputenv("DBUS_SESSION_BUS_ADDRESS", _originalBusAddress);
        }
    }

    void init()
    {
        for (const auto name : {"PATH", "XDG_CONFIG_HOME", "APPIMAGE", "FLATPAK_ID", "SNAP"}) {
            _environment.insert(name, qgetenv(name));
            qunsetenv(name);
        }
        _directory = std::make_unique<QTemporaryDir>();
        QVERIFY(_directory->isValid());
        QVERIFY(QDir().mkpath(_directory->filePath(u"config/autostart"_s)));
        qputenv("PATH", _directory->path().toUtf8());
        qputenv("XDG_CONFIG_HOME", _directory->filePath(u"config"_s).toUtf8());
        auto reset = false;
        configureManager([&](auto &manager) {
            reset = manager.reset();
        });
        QVERIFY(reset);
    }

    void cleanup()
    {
        verifyRequests();
        for (auto it = _environment.cbegin(); it != _environment.cend(); ++it) {
            if (it.value().isNull()) {
                qunsetenv(it.key().constData());
            } else {
                qputenv(it.key().constData(), it.value());
            }
        }
        _environment.clear();
        _directory.reset();
    }

    void serviceState_data()
    {
        QTest::addColumn<QString>("serviceState");
        QTest::addColumn<bool>("enabled");
        QTest::newRow("enabled") << u"enabled"_s << true;
        QTest::newRow("enabled-runtime") << u"enabled-runtime"_s << true;
        QTest::newRow("empty-response") << QString() << true;
        for (const auto state : {"disabled", "masked", "masked-runtime", "static"}) {
            QTest::newRow(state) << QString::fromLatin1(state) << false;
        }
    }

    void serviceState()
    {
        QFETCH(QString, serviceState);
        QFETCH(bool, enabled);
        configureManager([&](auto &manager) {
            manager.state = serviceState;
        });
        QCOMPARE(OCC::Utility::hasLaunchOnStartup(QString::fromLatin1(APPLICATION_SHORTNAME)), enabled);
        QCOMPARE(methodCount(u"GetUnitFileState"_s), 1);
    }

    void toggleBothStartupPaths_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::newRow("enable") << true;
        QTest::newRow("disable") << false;
    }

    void toggleBothStartupPaths()
    {
        QFETCH(bool, enabled);
        QVERIFY(writeFile(desktopPath(), "legacy desktop entry"));
        toggle(enabled);
        auto methods = QStringList{};
        for (const auto &request : requests()) {
            methods.append(request.member());
        }
        const auto expectedMethods = QStringList{u"GetUnitFileState"_s, u"MaskUnitFiles"_s, u"Reload"_s};
        QCOMPARE(methods, expectedMethods);
        QCOMPARE(state(), u"masked"_s);
        QCOMPARE(QFile::exists(desktopPath()), enabled);
        QCOMPARE(methodCount(u"MaskUnitFiles"_s), 1);
        QCOMPARE(methodCount(u"Reload"_s), 1);
        toggle(enabled);
        QCOMPARE(methodCount(u"MaskUnitFiles"_s), 1);
        QCOMPARE(methodCount(u"Reload"_s), 1);
        QCOMPARE(OCC::Utility::hasLaunchOnStartup(QString::fromLatin1(APPLICATION_SHORTNAME)), enabled);
        if (enabled) {
            QVERIFY(readFile(desktopPath()).contains(" --background\n"));
        }
    }

    void persistentMask_data()
    {
        QTest::addColumn<QString>("initialState");
        QTest::newRow("disabled") << u"disabled"_s;
        QTest::newRow("masked-runtime") << u"masked-runtime"_s;
    }

    void persistentMask()
    {
        QFETCH(QString, initialState);
        configureManager([&](auto &manager) {
            manager.state = initialState;
        });
        toggle(true);
        QCOMPARE(state(), u"masked"_s);
        QVERIFY(QFile::exists(desktopPath()));
        QCOMPARE(methodCount(u"MaskUnitFiles"_s), 1);
    }

    void emptyMaskReply()
    {
        configureManager([](auto &manager) {
            manager.emptyMaskReply = true;
        });
        toggle(true);
        QCOMPARE(state(), u"masked"_s);
        QVERIFY(QFile::exists(desktopPath()));
        QCOMPARE(methodCount(u"Reload"_s), 1);
    }

    void maskFailure_data()
    {
        QTest::addColumn<bool>("malformed");
        QTest::addColumn<bool>("enabled");
        QTest::newRow("denied-enable") << false << true;
        QTest::newRow("denied-disable") << false << false;
        QTest::newRow("malformed-enable") << true << true;
        QTest::newRow("malformed-disable") << true << false;
    }

    void maskFailure()
    {
        QFETCH(bool, malformed);
        QFETCH(bool, enabled);
        configureManager([&](auto &manager) {
            manager.maskFailure = !malformed;
            manager.malformedMaskReply = malformed;
        });
        if (!enabled) {
            QVERIFY(writeFile(desktopPath(), "legacy desktop entry"));
        }
        toggle(enabled);
        QVERIFY(!QFile::exists(desktopPath()));
        QCOMPARE(state(), u"enabled"_s);
        QVERIFY(OCC::Utility::hasLaunchOnStartup(QString::fromLatin1(APPLICATION_SHORTNAME)));
        QCOMPARE(methodCount(u"Reload"_s), 0);
    }

    void reloadFailureAfterMask_data()
    {
        QTest::addColumn<bool>("malformed");
        QTest::newRow("error") << false;
        QTest::newRow("malformed") << true;
    }

    void reloadFailureAfterMask()
    {
        QFETCH(bool, malformed);
        configureManager([&](auto &manager) {
            manager.reloadFailure = !malformed;
            manager.malformedReloadReply = malformed;
        });
        toggle(true);
        QCOMPARE(state(), u"masked"_s);
        QVERIFY(QFile::exists(desktopPath()));
        QVERIFY(OCC::Utility::hasLaunchOnStartup(QString::fromLatin1(APPLICATION_SHORTNAME)));
        QCOMPARE(methodCount(u"Reload"_s), 1);
    }

    void queryFailure_data()
    {
        QTest::addColumn<QString>("failure");
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<bool>("existingDesktop");
        for (const auto failure : {"timeout", "error", "malformed", "extra-argument", "disconnected", "no-server"}) {
            for (const auto enabled : {true, false}) {
                for (const auto existing : {true, false}) {
                    const auto name = QString::fromLatin1(failure) + (enabled ? u"-enable"_s : u"-disable"_s) + (existing ? u"-existing"_s : u"-absent"_s);
                    QTest::newRow(qPrintable(name)) << QString::fromLatin1(failure) << enabled << existing;
                }
            }
        }
    }

    void queryFailure()
    {
        QFETCH(QString, failure);
        QFETCH(bool, enabled);
        QFETCH(bool, existingDesktop);
        configureManager([&](auto &manager) {
            manager.queryTimeout = failure == u"timeout";
            manager.malformedQueryReply = failure == u"malformed";
            manager.extraQueryArgument = failure == u"extra-argument";
            if (failure == u"error") {
                manager.queryError = u"org.freedesktop.DBus.Error.AccessDenied"_s;
            } else if (failure == u"disconnected") {
                manager.queryError = u"org.freedesktop.DBus.Error.Disconnected"_s;
            } else if (failure == u"no-server") {
                manager.queryError = u"org.freedesktop.DBus.Error.NoServer"_s;
            }
        });
        if (existingDesktop) {
            QVERIFY(writeFile(desktopPath(), "legacy desktop entry"));
        }
        QVERIFY(OCC::Utility::hasLaunchOnStartup(QString::fromLatin1(APPLICATION_SHORTNAME)));
        auto timer = QElapsedTimer();
        timer.start();
        toggle(enabled);
        QVERIFY(timer.elapsed() < maxCommandDurationMs);
        QCOMPARE(QFile::exists(desktopPath()), existingDesktop && enabled);
        if (existingDesktop && enabled) {
            QCOMPARE(readFile(desktopPath()), QByteArray("legacy desktop entry"));
        }
        QCOMPARE(state(), u"enabled"_s);
        QCOMPARE(methodCount(u"MaskUnitFiles"_s), 0);
        QCOMPARE(methodCount(u"Reload"_s), 0);
        QVERIFY(OCC::Utility::hasLaunchOnStartup(QString::fromLatin1(APPLICATION_SHORTNAME)));
    }

    void initiallyDisconnectedBus_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<bool>("existingDesktop");
        QTest::newRow("enable-existing") << true << true;
        QTest::newRow("enable-absent") << true << false;
        QTest::newRow("disable-existing") << false << true;
        QTest::newRow("disable-absent") << false << false;
    }

    void initiallyDisconnectedBus()
    {
        QFETCH(bool, enabled);
        QFETCH(bool, existingDesktop);
        if (existingDesktop) {
            QVERIFY(writeFile(desktopPath(), "legacy desktop entry"));
        }
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(u"DBUS_SESSION_BUS_ADDRESS"_s, u"unix:path="_s + _directory->filePath(u"missing-bus"_s));
        auto child = QProcess();
        child.setProcessEnvironment(environment);
        child.start(QCoreApplication::applicationFilePath(), {u"--disconnected-bus-child"_s, enabled ? u"enable"_s : u"disable"_s});
        const auto finished = child.waitForFinished(maxCommandDurationMs);
        if (!finished) {
            child.kill();
            child.waitForFinished(maxCommandDurationMs);
        }
        QVERIFY(finished);
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
        QVERIFY(requests().isEmpty());
    }

    void xdgFallback_data()
    {
        QTest::addColumn<QByteArray>("skipReason");
        for (const auto reason : {"missing-service", "missing-unit", "APPIMAGE", "FLATPAK_ID", "SNAP", "unrelated-app"}) {
            QTest::newRow(reason) << QByteArray(reason);
        }
    }

    void xdgFallback()
    {
        QFETCH(QByteArray, skipReason);
        auto appName = QString::fromLatin1(APPLICATION_SHORTNAME);
        if (skipReason == "missing-service") {
            configureManager([](auto &manager) {
                manager.unregisterService();
            });
        } else if (skipReason == "missing-unit") {
            configureManager([](auto &manager) {
                manager.queryError = u"org.freedesktop.DBus.Error.FileNotFound"_s;
            });
        } else if (skipReason == "unrelated-app") {
            appName = u"OtherSyncApp"_s;
        } else {
            const auto portablePath = _directory->filePath(u"portable-app"_s);
            QVERIFY(writeFile(portablePath));
            qputenv(skipReason.constData(), portablePath.toUtf8());
        }
        QVERIFY(!OCC::Utility::hasLaunchOnStartup(appName));
        toggle(true, appName);
        QVERIFY(OCC::Utility::hasLaunchOnStartup(appName));
        QVERIFY(QFile::exists(desktopPath(appName)));
        toggle(false, appName);
        QVERIFY(!OCC::Utility::hasLaunchOnStartup(appName));
        QVERIFY(!QFile::exists(desktopPath(appName)));
        QCOMPARE(methodCount(u"MaskUnitFiles"_s), 0);
        if (skipReason != "missing-unit") {
            QVERIFY(requests().isEmpty());
        }
    }
};

int main(int argc, char **argv)
{
    auto application = QCoreApplication(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() == 3 && arguments.at(1) == u"--disconnected-bus-child") {
        if (QDBusConnection::sessionBus().isConnected()) {
            return 1;
        }
        const auto appName = QString::fromLatin1(APPLICATION_SHORTNAME);
        const auto path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + u"/autostart/"_s + appName + u".desktop"_s;
        auto desktop = QFile(path);
        const auto existed = desktop.exists();
        const auto enabled = arguments.at(2) == u"enable";
        if (!OCC::Utility::hasLaunchOnStartup(appName)) {
            return 2;
        }
        OCC::Utility::setLaunchOnStartup(appName, appName, enabled);
        if (desktop.exists() != (existed && enabled)) {
            return 3;
        }
        if (existed && enabled && (!desktop.open(QIODevice::ReadOnly) || desktop.readAll() != "legacy desktop entry")) {
            return 4;
        }
        return OCC::Utility::hasLaunchOnStartup(appName) ? 0 : 5;
    }
    auto test = TestLaunchOnStartup();
    return QTest::qExec(&test, argc, argv);
}

#include "testlaunchonstartup.moc"
