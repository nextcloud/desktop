/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include <QTemporaryDir>

#include "configfile.h"
#include "theme.h"

using namespace OCC;

namespace {

/// Writes the given key/value pairs into the config file ConfigFile will read.
void writeConfig(const QString &confDir, const QVariantMap &values)
{
    QSettings settings(confDir + QLatin1Char('/') + Theme::instance()->configFileName(), QSettings::IniFormat);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.sync();
}

}

/// Covers the resolution of the sync tuning knobs: config file first, environment variable as an
/// override, documented default when neither says anything usable.
///
/// Each accessor reads the config file on every call rather than caching, so a single test process
/// can rewrite the config between checks. The call sites cache in a function-local static, which is
/// why the values are read here through ConfigFile directly.
class TestConfigFile : public QObject
{
    Q_OBJECT

    QTemporaryDir _confDir;

private slots:
    void init()
    {
        QVERIFY(_confDir.isValid());
        ConfigFile::setConfDir(_confDir.path());
        // Start every case from an empty config file.
        QFile::remove(_confDir.path() + QLatin1Char('/') + Theme::instance()->configFileName());
    }

    void cleanup()
    {
        qunsetenv("OWNCLOUD_DISCOVERY_TIMEOUT");
        qunsetenv("OWNCLOUD_DISCOVERY_LISTING_RETRIES");
        qunsetenv("OWNCLOUD_CONNECTION_TIMEOUT_RESET_SEC");
        qunsetenv("OWNCLOUD_PROPAGATE_503_RETRIES");
    }

    /// Nothing configured anywhere: every knob reports its documented default.
    void testDefaults()
    {
        ConfigFile cfg;
        QCOMPARE(cfg.discoveryTimeout(), std::chrono::seconds(60));
        QCOMPARE(cfg.discoveryListingRetries(), 3);
        QCOMPARE(cfg.discoveryListingRetryResetInterval(), std::chrono::seconds(300));
        QCOMPARE(cfg.discoveryListingRetryDelay(), std::chrono::seconds(5));
        QCOMPARE(cfg.connectionTimeoutRetries(), 3);
        QCOMPARE(cfg.connectionTimeoutResetInterval(), std::chrono::seconds(300));
        QCOMPARE(cfg.propagateServiceUnavailableRetries(), 3);
        QCOMPARE(cfg.propagateServiceUnavailableResetInterval(), std::chrono::seconds(60));
        QCOMPARE(cfg.propagateServiceUnavailableBackoff(), std::chrono::seconds(5));
        QCOMPARE(cfg.maxParallelLocalScanJobs(), 0);
    }

    /// The whole point of the change: a value in the config file is what takes effect.
    void testReadFromConfigFile()
    {
        writeConfig(_confDir.path(),
            {{QStringLiteral("discoveryTimeout"), 150},
                {QStringLiteral("discoveryListingRetries"), 5},
                {QStringLiteral("discoveryListingRetryResetInterval"), 600},
                {QStringLiteral("discoveryListingRetryDelay"), 7},
                {QStringLiteral("connectionTimeoutRetries"), 4},
                {QStringLiteral("connectionTimeoutResetInterval"), 900},
                {QStringLiteral("propagateServiceUnavailableRetries"), 6},
                {QStringLiteral("propagateServiceUnavailableResetInterval"), 90},
                {QStringLiteral("propagateServiceUnavailableBackoff"), 2},
                {QStringLiteral("maxParallelLocalScanJobs"), 8}});

        ConfigFile cfg;
        QCOMPARE(cfg.discoveryTimeout(), std::chrono::seconds(150));
        QCOMPARE(cfg.discoveryListingRetries(), 5);
        QCOMPARE(cfg.discoveryListingRetryResetInterval(), std::chrono::seconds(600));
        QCOMPARE(cfg.discoveryListingRetryDelay(), std::chrono::seconds(7));
        QCOMPARE(cfg.connectionTimeoutRetries(), 4);
        QCOMPARE(cfg.connectionTimeoutResetInterval(), std::chrono::seconds(900));
        QCOMPARE(cfg.propagateServiceUnavailableRetries(), 6);
        QCOMPARE(cfg.propagateServiceUnavailableResetInterval(), std::chrono::seconds(90));
        QCOMPARE(cfg.propagateServiceUnavailableBackoff(), std::chrono::seconds(2));
        QCOMPARE(cfg.maxParallelLocalScanJobs(), 8);
    }

    /// The environment variable still wins, so a single run can be retuned without touching the
    /// config -- which is what the sync tests rely on to neuter a retry budget.
    void testEnvironmentOverridesConfigFile()
    {
        writeConfig(_confDir.path(),
            {{QStringLiteral("discoveryTimeout"), 150},
                {QStringLiteral("propagateServiceUnavailableRetries"), 6}});

        qputenv("OWNCLOUD_DISCOVERY_TIMEOUT", "42");
        qputenv("OWNCLOUD_PROPAGATE_503_RETRIES", "1");

        ConfigFile cfg;
        QCOMPARE(cfg.discoveryTimeout(), std::chrono::seconds(42));
        QCOMPARE(cfg.propagateServiceUnavailableRetries(), 1);
        // Untouched by the environment, so still the config file's value.
        QCOMPARE(cfg.discoveryListingRetries(), 3);
    }

    /// A zero or negative entry means "not configured" rather than "no attempts": a stray 0 in the
    /// config must not turn a retry budget into an immediate failure.
    void testNonPositiveValuesFallBackToDefault()
    {
        writeConfig(_confDir.path(),
            {{QStringLiteral("discoveryListingRetries"), 0},
                {QStringLiteral("propagateServiceUnavailableRetries"), -1},
                {QStringLiteral("discoveryTimeout"), QStringLiteral("not a number")}});

        ConfigFile cfg;
        QCOMPARE(cfg.discoveryListingRetries(), 3);
        QCOMPARE(cfg.propagateServiceUnavailableRetries(), 3);
        QCOMPARE(cfg.discoveryTimeout(), std::chrono::seconds(60));
    }

    /// The connection-timeout window has a 60s floor. A check that times out takes as long as its
    /// own timeout to do so, so consecutive failures are minutes apart by construction and a
    /// shorter window would reset the count every time, making the tolerance a no-op.
    void testConnectionTimeoutResetHasFloor()
    {
        writeConfig(_confDir.path(), {{QStringLiteral("connectionTimeoutResetInterval"), 5}});
        QCOMPARE(ConfigFile().connectionTimeoutResetInterval(), std::chrono::seconds(60));

        qputenv("OWNCLOUD_CONNECTION_TIMEOUT_RESET_SEC", "5");
        QCOMPARE(ConfigFile().connectionTimeoutResetInterval(), std::chrono::seconds(60));
    }
};

QTEST_APPLESS_MAIN(TestConfigFile)
#include "testconfigfile.moc"
