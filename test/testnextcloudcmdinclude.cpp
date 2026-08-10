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

class TestNextcloudCmdInclude : public QObject
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
    // --include  with not existent file must print a descriptive error and exit 1.
    void testIncludeWithInvalidPathPrintsError()
    {
        auto expectedExitCode = 1;

        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--non-interactive"), QStringLiteral("--include"), QStringLiteral("foo"), QStringLiteral("\"http://127.0.0.1:1\"")}, &exitCode);

        QCOMPARE(exitCode, expectedExitCode);
        QVERIFY2(output.contains("does not exist"), output.constData());
    }

    // --include with existent, readeable file, but in the wrong format, must print a descriptive error and exit 1.
    void testExistentAndReadableIncludeFileShouldNotParseIfFormatIsWrong()
    {
        auto expectedExitCode = 1;

        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--non-interactive"), QStringLiteral("--include"), QStringLiteral("NextcloudCmdIncludeTest"), QStringLiteral("\"http://user:user@127.0.0.1:1\"")}, &exitCode);

        QCOMPARE(exitCode, expectedExitCode);
        QVERIFY2(output.contains("is not in format"), output.constData());
    }

    // --include with existent and readeable file must not print an error.
    void testExistentAndReadableIncludeFileShouldParseIfFormatIsCorrect()
    {
        auto expectedExitCode = 1;

        auto testFile = QFile("foo");
        if (testFile.open(QIODevice::ReadWrite)) {
            auto stream = QTextStream(&testFile);
            stream << ". /foo" << Qt::endl;
        }


        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--non-interactive"), QStringLiteral("--include"), QStringLiteral("foo"), QStringLiteral("\"http://user:user@127.0.0.1:1\"")}, &exitCode);

        testFile.remove();
        QCOMPARE(exitCode, expectedExitCode);
        QVERIFY2(!output.contains("does not exist"), output.constData());
        QVERIFY2(!output.contains("is not in format"), output.constData());
        QVERIFY2(output.contains("server"), output.constData());
    }

    // --include with <source_dir> must print a descriptive error and exit 0.
    void testValidIncludeFileWithSourceDir()
    {
        auto expectedExitCode = 0;

        auto testFile = QFile("foo");
        if (testFile.open(QIODevice::ReadWrite)) {
            auto stream = QTextStream(&testFile);
            stream << ". /foo" << Qt::endl;
        }


        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--non-interactive"), QStringLiteral("--include"), QStringLiteral("foo"), QStringLiteral("bar"),QStringLiteral("\"http://user:user@127.0.0.1:1\"")}, &exitCode);

        testFile.remove();
        QCOMPARE(exitCode, expectedExitCode);
        QVERIFY2(output.contains("--include"), output.constData());
    }
    
    // <source_dir> without --include not print an error.
    void testSourceDirWithoutInclude()
    {
        auto expectedExitCode = 1;

        int exitCode = 0;
        const auto output = runCmd({QStringLiteral("--non-interactive"), QStringLiteral("."), QStringLiteral("\"http://user:user@127.0.0.1:1\"")}, &exitCode);

        QCOMPARE(exitCode, expectedExitCode);
        QVERIFY2(!output.contains("does not exist"), output.constData());
        QVERIFY2(!output.contains("is not in format"), output.constData());
        QVERIFY2(output.contains("server"), output.constData());
    }

    // No arguments: help text is printed, exit 0.  Guards against regressions.
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
        QVERIFY2(output.contains("--include"), output.constData());
    }
};

QTEST_GUILESS_MAIN(TestNextcloudCmdInclude)
#include "testnextcloudcmdinclude.moc"
