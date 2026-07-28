/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include <QtTest>
#include <QProcess>
#include <QString>
#include <QByteArray>

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
        proc.waitForFinished(timeoutMs);
        if (exitCodeOut) {
            *exitCodeOut = proc.exitCode();
        }
        return proc.readAll();
    }

private slots:
    // --userid without --serverurl must print a descriptive error and exit 1.
    // It must NOT print the interactive "Please enter username:" prompt.
    void testUserIdAlonePrintsServerUrlError()
    {
        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--userid"), QStringLiteral("alice")}, &exitCode);

        QCOMPARE(exitCode, 255);
        QVERIFY2(output.contains("--serverurl"), output.constData());
        QVERIFY2(!output.contains("Please enter username"), output.constData());
    }

    // --userid + --serverurl without --apppassword must enter provisioning mode
    // (app password is optional, as in the nextcloud GUI executable).
    // No credential prompt must appear.
    void testUserIdAndServerUrlWithoutAppPasswordEntersProvisionMode()
    {
        // 127.0.0.1:1 gives a fast "connection refused" without DNS latency.
        int exitCode = 0;
        const auto output = runCmd(
            {QStringLiteral("--userid"), QStringLiteral("alice"),
             QStringLiteral("--serverurl"), QStringLiteral("http://127.0.0.1:1")},
            &exitCode);

        QCOMPARE(exitCode, 255);
        // Must NOT fall through into interactive sync mode.
        QVERIFY2(!output.contains("Please enter username"), output.constData());
        QVERIFY2(!output.contains("Password for account with username"), output.constData());
    }

    // --userid + --serverurl (+ optional --apppassword) pointing at an unreachable
    // server: the command must enter provisioning mode (no credential prompt)
    // and fail with an error, not fall through into interactive sync mode.
    void testProvisioningOptionsEnterProvisionModeNotSyncMode()
    {
        // 127.0.0.1:1 gives a fast "connection refused" without DNS latency.
        int exitCode = 0;
        const auto output = runCmd(
            {QStringLiteral("--userid"), QStringLiteral("alice"),
             QStringLiteral("--apppassword"), QStringLiteral("secret"),
             QStringLiteral("--serverurl"), QStringLiteral("http://127.0.0.1:1")},
            &exitCode);

        QCOMPARE(exitCode, 255);
        // Provisioning mode must NOT fall through into interactive sync mode.
        QVERIFY2(!output.contains("Please enter username"), output.constData());
        QVERIFY2(!output.contains("Password for account with username"), output.constData());
    }

    // No arguments: help text is printed, exit 0.  Guards against regressions
    // in the normal (non-provisioning) sync-mode entry path.
    void testNoArgsShowsHelp()
    {
        int exitCode = -1;
        const auto output = runCmd({}, &exitCode);

        QCOMPARE(exitCode, 0);
        QVERIFY2(output.contains("nextcloudcmd") || output.contains("nextclouddevcmd"),
                 output.constData());
        QVERIFY2(output.contains("--userid"), output.constData());
    }

    // --non-interactive combined with --userid (but missing the other two
    // provisioning options) must still print a structured error, not a prompt.
    void testNonInteractiveFlagDoesNotSuppressProvisioningError()
    {
        int exitCode = 0;
        const auto output = runCmd(
            {QStringLiteral("--non-interactive"),
             QStringLiteral("--userid"), QStringLiteral("alice")},
            &exitCode);

        QCOMPARE(exitCode, 255);
        QVERIFY2(output.contains("--serverurl"), output.constData());
        QVERIFY2(!output.contains("Please enter username"), output.constData());
    }
};

QTEST_GUILESS_MAIN(TestNextcloudCmdProvisioning)
#include "testnextcloudcmdprovisioning.moc"
