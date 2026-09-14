/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

#include "config.h"

// The binary is named APPLICATION_EXECUTABLEcmd and lives in OWNCLOUD_BIN_PATH.
// Both macros are provided by the build system at compile time.
static QString nextcloudCmdBinary()
{
    return QString::fromLatin1(OWNCLOUD_BIN_PATH "/" APPLICATION_EXECUTABLE "cmd");
}

class TestNextcloudCmdProvisioning : public QObject
{
    Q_OBJECT

private:
    // Runs nextcloudcmd with the given arguments, waits up to timeoutMs for it
    // to finish, and returns the combined stdout+stderr output together with the
    // exit code via out-parameters.  stdin of the child process is closed
    // immediately so that any accidental interactive read returns EOF rather
    // than blocking the test suite.
    static QByteArray runCmd(const QStringList &args, int *exitCodeOut, int timeoutMs = 12000)
    {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(nextcloudCmdBinary(), args);
        proc.closeWriteChannel(); // close stdin — no interactive reads
        if (!proc.waitForFinished(timeoutMs)) {
            // Kill rather than let the QProcess destructor block on a wedged child, and
            // leave an exit code behind that no expectation below accepts.
            proc.kill();
            proc.waitForFinished();
        }
        if (exitCodeOut) {
            *exitCodeOut = proc.exitCode();
        }
        return proc.readAll();
    }

    // Returns true once an account section has been written to the configuration in
    // confDir.  A failed setup must leave the configuration without one.
    static bool configHasAccount(const QString &confDir)
    {
        const QDir dir(confDir);
        const auto configFileNames = dir.entryList({QStringLiteral("*.cfg")}, QDir::Files);

        for (const auto &configFileName : configFileNames) {
            QFile configFile(dir.filePath(configFileName));
            if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }

            if (configFile.readAll().contains("webflow_user")) {
                return true;
            }
        }

        return false;
    }

    // A port nothing listens on, so the provisioning setup always fails on the first
    // request instead of depending on a server being available to the test.
    static QString unreachableServerUrl()
    {
        return QStringLiteral("http://127.0.0.1:1");
    }

private Q_SLOTS:
    // --userid without --serverurl must print a descriptive error and exit 1.
    // It must NOT print the interactive "Please enter username:" prompt.
    void testUserIdAlonePrintsServerUrlError()
    {
#if defined Q_OS_WINDOWS
        constexpr auto expectedExitCode = -1;
#else
        constexpr auto expectedExitCode = 255;
#endif

        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--userid"), QStringLiteral("alice")}, &exitCode);

        QCOMPARE(exitCode, expectedExitCode);
        QVERIFY2(output.contains("--serverurl"), output.constData());
        QVERIFY2(!output.contains("Please enter username"), output.constData());
    }

    // --userid + --serverurl without --apppassword must enter provisioning mode
    // (app password is optional, as in the nextcloud GUI executable).
    // No credential prompt must appear.
    void testUserIdAndServerUrlWithoutAppPasswordEntersProvisionMode()
    {
#if defined Q_OS_WINDOWS
        constexpr auto expectedExitCode = -1;
#else
        constexpr auto expectedExitCode = 255;
#endif

        // 127.0.0.1:1 gives a fast "connection refused" without DNS latency.
        int exitCode = 0;
        const auto output = runCmd(
            {QStringLiteral("--userid"), QStringLiteral("alice"),
             QStringLiteral("--serverurl"), QStringLiteral("\"http://127.0.0.1:1\"")},
            &exitCode);

        QCOMPARE(exitCode, expectedExitCode);
        // Must NOT fall through into interactive sync mode.
        QVERIFY2(!output.contains("Please enter username"), output.constData());
        QVERIFY2(!output.contains("Password for account with username"), output.constData());
    }

    // --userid + --serverurl (+ optional --apppassword) pointing at an unreachable
    // server: the command must enter provisioning mode (no credential prompt)
    // and fail with an error, not fall through into interactive sync mode.
    void testProvisioningOptionsEnterProvisionModeNotSyncMode()
    {
#if defined Q_OS_WINDOWS
        constexpr auto expectedExitCode = -1;
#else
        constexpr auto expectedExitCode = 255;
#endif

        // 127.0.0.1:1 gives a fast "connection refused" without DNS latency.
        int exitCode = 0;
        const auto output = runCmd(
            {QStringLiteral("--userid"), QStringLiteral("alice"),
             QStringLiteral("--apppassword"), QStringLiteral("secret"),
             QStringLiteral("--serverurl"), QStringLiteral("\"http://127.0.0.1:1\"")},
            &exitCode);

        QCOMPARE(exitCode, expectedExitCode);
        // Provisioning mode must NOT fall through into interactive sync mode.
        QVERIFY2(!output.contains("Please enter username"), output.constData());
        QVERIFY2(!output.contains("Password for account with username"), output.constData());
    }

    // No arguments: help text is printed, exit 0.  Guards against regressions
    // in the normal (non-provisioning) sync-mode entry path.
    void testNoArgsShowsHelp()
    {
#if defined Q_OS_MACOS
        constexpr auto expectedExitCode = 255;
#else
        constexpr auto expectedExitCode = 0;
#endif
        int exitCode = -1;
        const auto output = runCmd({}, &exitCode);

        QCOMPARE(exitCode, expectedExitCode);
        QVERIFY2(output.contains("nextcloudcmd") || output.contains("nextclouddevcmd"),
                 output.constData());
        QVERIFY2(output.contains("--userid"), output.constData());
    }

    // --non-interactive combined with --userid (but missing the other two
    // provisioning options) must still print a structured error, not a prompt.
    void testNonInteractiveFlagDoesNotSuppressProvisioningError()
    {
#if defined Q_OS_WINDOWS
        constexpr auto expectedExitCode = -1;
#else
        constexpr auto expectedExitCode = 255;
#endif

        int exitCode = 0;
        const auto output = runCmd(
            {QStringLiteral("--non-interactive"),
             QStringLiteral("--userid"), QStringLiteral("alice")},
            &exitCode);

        QCOMPARE(exitCode, expectedExitCode);
        QVERIFY2(output.contains("--serverurl"), output.constData());
        QVERIFY2(!output.contains("Please enter username"), output.constData());
    }

    // The same as testUserIdAlonePrintsServerUrlError, but with the inline
    // "--option=value" spelling.  Provisioning mode is selected by looking for
    // --userid in the arguments, so before the inline form was expanded this fell
    // through into sync mode and printed the generic help instead.
    void testInlineOptionValuesSelectProvisioningMode()
    {
#if defined Q_OS_WINDOWS
        constexpr auto expectedExitCode = -1;
#else
        constexpr auto expectedExitCode = 255;
#endif

        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--userid=alice")}, &exitCode);

        QCOMPARE(exitCode, expectedExitCode);
        QVERIFY2(output.contains("--serverurl"), output.constData());
        QVERIFY2(!output.contains("Please enter username"), output.constData());
    }

    // A full set of provisioning options in the inline spelling has to reach the
    // account setup.  Sync mode would instead have taken the last two arguments as
    // the positional source directory and server URL and complained about the
    // source directory not existing.
    void testInlineOptionValuesReachAccountSetup()
    {
        QTemporaryDir confDir;
        QVERIFY(confDir.isValid());

        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--confdir"),
                                    confDir.path(),
                                    QStringLiteral("--userid=alice"),
                                    QStringLiteral("--apppassword=secret"),
                                    QStringLiteral("--serverurl=") + unreachableServerUrl()},
                                   &exitCode);

        QVERIFY2(output.contains("Could not fetch username"), output.constData());
        QVERIFY2(!output.contains("does not exist"), output.constData());
        QCOMPARE(exitCode, 1);
    }

    // --localdirpath is documented as optional.  Leaving it out used to abort the
    // setup right away with "Could not create local folder because the name is
    // empty", so the credentials were never even sent to the server.
    void testSetupWithoutLocalDirPathIsNotRejected()
    {
        QTemporaryDir confDir;
        QVERIFY(confDir.isValid());

        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--confdir"),
                                    confDir.path(),
                                    QStringLiteral("--userid"),
                                    QStringLiteral("alice"),
                                    QStringLiteral("--apppassword"),
                                    QStringLiteral("secret"),
                                    QStringLiteral("--serverurl"),
                                    unreachableServerUrl()},
                                   &exitCode);

        QVERIFY2(!output.contains("local folder because the name is empty"), output.constData());
        // The setup got as far as contacting the server, which is where it fails here.
        QVERIFY2(output.contains("Could not fetch username"), output.constData());
        QCOMPARE(exitCode, 1);
    }

    // With an app password the setup is asynchronous: the credentials are checked
    // against the server before the account is written.  nextcloudcmd has to run an
    // event loop for any of that to happen — without one it returned success before
    // the first reply arrived.  So an unreachable server has to be reported as a
    // failure, and nothing may be left in the configuration.
    void testAppPasswordSetupRunsTheEventLoop()
    {
        QTemporaryDir confDir;
        QVERIFY(confDir.isValid());

        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--confdir"),
                                    confDir.path(),
                                    QStringLiteral("--userid"),
                                    QStringLiteral("alice"),
                                    QStringLiteral("--apppassword"),
                                    QStringLiteral("secret"),
                                    QStringLiteral("--serverurl"),
                                    unreachableServerUrl()},
                                   &exitCode);

        // 1 is the code the setup job exits with.  The options were accepted, so this
        // is neither the 255 of a rejected command line nor the 0 of an early return
        // that never waited for the result.
        QCOMPARE(exitCode, 1);
        QVERIFY2(output.contains("Could not fetch username"), output.constData());
        QVERIFY2(!configHasAccount(confDir.path()), "a failed setup must not store an account");
    }
};

QTEST_GUILESS_MAIN(TestNextcloudCmdProvisioning)
#include "testnextcloudcmdprovisioning.moc"
