/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDeadlineTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <QtTest>

#include <KDSingleApplication>

#include <cstdio>
#include <memory>
#include <vector>

#include "gui/linuxsingleapplication.h"

using namespace Qt::StringLiterals;

class TestLinuxSingleApplication : public QObject
{
    Q_OBJECT

    static constexpr auto processTimeoutMs = 5000;

    std::unique_ptr<QTemporaryDir> _directory;
    std::vector<std::unique_ptr<QProcess>> _processes;

    QProcess *startChild(const QString &name, const QString &session, bool legacy = false, const QString &message = {})
    {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(u"TMPDIR"_s, _directory->path());
        if (session.isNull()) {
            environment.remove(u"XDG_SESSION_ID"_s);
        } else {
            environment.insert(u"XDG_SESSION_ID"_s, session);
        }
        auto process = std::make_unique<QProcess>();
        process->setProcessEnvironment(environment);
        const auto mode = legacy ? u"--legacy-child"_s : u"--child"_s;
        process->start(QCoreApplication::applicationFilePath(), {mode, name, message});
        _processes.push_back(std::move(process));
        return _processes.back().get();
    }

    [[nodiscard]] QByteArray readLine(QProcess *process) const
    {
        auto deadline = QDeadlineTimer(processTimeoutMs);
        while (!process->canReadLine() && !deadline.hasExpired()) {
            if (!process->waitForReadyRead(deadline.remainingTime())) {
                break;
            }
        }
        return process->readLine().trimmed();
    }

    void stopChild(QProcess *process) const
    {
        process->write("quit\n");
        QVERIFY(process->waitForFinished(processTimeoutMs));
        QCOMPARE(process->exitStatus(), QProcess::NormalExit);
        QCOMPARE(process->exitCode(), 0);
    }

    void checkSecondary(QProcess *secondary) const
    {
        QCOMPARE(readLine(secondary), QByteArray("SECONDARY"));
        QVERIFY(secondary->state() == QProcess::NotRunning || secondary->waitForFinished(processTimeoutMs));
        QCOMPARE(secondary->exitStatus(), QProcess::NormalExit);
        QCOMPARE(secondary->exitCode(), 0);
    }

    void checkForwarding(QProcess *primary, const QString &name, const QString &session, bool legacy = false)
    {
        QCOMPARE(readLine(primary), QByteArray("PRIMARY"));
        const auto message = u"forwarded action"_s;
        checkSecondary(startChild(name, session, legacy, message));
        QCOMPARE(readLine(primary), QByteArray("MESSAGE ") + message.toUtf8().toHex());
        QCOMPARE(primary->state(), QProcess::Running);
    }

private Q_SLOTS:
    void init()
    {
        _directory = std::make_unique<QTemporaryDir>(QDir::tempPath() + u"/nc-si-XXXXXX"_s);
        QVERIFY(_directory->isValid());
    }

    void cleanup()
    {
        for (const auto &process : _processes) {
            if (process->state() != QProcess::NotRunning) {
                process->write("quit\n");
                if (!process->waitForFinished(processTimeoutMs)) {
                    process->kill();
                    process->waitForFinished(processTimeoutMs);
                }
            }
        }
        _processes.clear();
        _directory.reset();
    }

    void sharedAcrossSessions_data()
    {
        QTest::addColumn<QString>("firstSession");
        QTest::addColumn<QString>("secondSession");
        QTest::newRow("unset-to-set") << QString() << u"session-1"_s;
        QTest::newRow("set-to-unset") << u"session-1"_s << QString();
        QTest::newRow("different-sessions") << u"session-1"_s << u"session-2"_s;
        QTest::newRow("same-session") << u"session-1"_s << u"session-1"_s;
        QTest::newRow("both-unset") << QString() << QString();
    }

    void sharedAcrossSessions()
    {
        QFETCH(QString, firstSession);
        QFETCH(QString, secondSession);
        const auto identity = u"nextcloud-session-test"_s;
        checkForwarding(startChild(identity, firstSession), identity, secondSession);
    }

    void legacyCompatibility_data()
    {
        QTest::addColumn<bool>("legacyFirst");
        QTest::addColumn<QString>("firstSession");
        QTest::addColumn<QString>("secondSession");
        const auto session = u"session-1"_s;
        QTest::newRow("legacy-to-current-same-session") << true << session << session;
        QTest::newRow("current-to-legacy-same-session") << false << session << session;
        QTest::newRow("legacy-to-current-both-unset") << true << QString() << QString();
        QTest::newRow("current-to-legacy-both-unset") << false << QString() << QString();
        QTest::newRow("legacy-to-current-unset-to-set") << true << QString() << session;
        QTest::newRow("current-to-legacy-set-to-unset") << false << session << QString();
    }

    void legacyCompatibility()
    {
        QFETCH(bool, legacyFirst);
        QFETCH(QString, firstSession);
        QFETCH(QString, secondSession);
        const auto identity = u"nextcloud-legacy-test"_s;
        checkForwarding(startChild(identity, firstSession, legacyFirst), identity, secondSession, !legacyFirst);
    }

    void simultaneousLaunchesWithLegacyPrimary()
    {
        const auto identity = u"nextcloud-legacy-test"_s;
        const auto session = u"session-1"_s;
        auto primary = startChild(identity, session, true);
        QCOMPARE(readLine(primary), QByteArray("PRIMARY"));

        const auto firstMessage = u"first action"_s;
        const auto secondMessage = u"second action"_s;
        auto first = startChild(identity, session, false, firstMessage);
        auto second = startChild(identity, session, false, secondMessage);
        checkSecondary(first);
        checkSecondary(second);

        const auto received = QSet{readLine(primary), readLine(primary)};
        const auto expected = QSet{QByteArray("MESSAGE ") + firstMessage.toUtf8().toHex(), QByteArray("MESSAGE ") + secondMessage.toUtf8().toHex()};
        QCOMPARE(received, expected);
        QCOMPARE(primary->state(), QProcess::Running);
    }

    void takeoverAfterExit()
    {
        const auto identity = u"nextcloud-takeover-test"_s;
        auto first = startChild(identity, u"session-1"_s);
        QCOMPARE(readLine(first), QByteArray("PRIMARY"));
        stopChild(first);
        auto replacement = startChild(identity, u"session-2"_s);
        QCOMPARE(readLine(replacement), QByteArray("PRIMARY"));
        stopChild(replacement);
    }

    void distinctIdentities()
    {
        auto first = startChild(u"first-brand"_s, u"session-1"_s);
        auto second = startChild(u"second-brand"_s, u"session-1"_s);
        QCOMPARE(readLine(first), QByteArray("PRIMARY"));
        QCOMPARE(readLine(second), QByteArray("PRIMARY"));
        QCOMPARE(first->state(), QProcess::Running);
        QCOMPARE(second->state(), QProcess::Running);
    }
};

template<typename Guard>
int runChild(QCoreApplication &application, Guard &guard, const QString &message)
{
    constexpr auto childLifetimeMs = 15000;
    auto output = QTextStream(stdout);
    if (!guard.isPrimaryInstance()) {
        if (!guard.sendMessage(message.toUtf8())) {
            return 2;
        }
        output << u"SECONDARY\n"_s << Qt::flush;
        return 0;
    }
    QObject::connect(&guard, &Guard::messageReceived, &application, [&](const auto &receivedMessage) {
        output << u"MESSAGE "_s << QString::fromLatin1(receivedMessage.toHex()) << Qt::endl;
    });
    auto input = QFile();
    if (!input.open(stdin, QIODevice::ReadOnly)) {
        return 2;
    }
    auto notifier = QSocketNotifier(fileno(stdin), QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, &application, [&] {
        if (input.readLine().trimmed() == "quit") {
            application.quit();
        }
    });
    QTimer::singleShot(childLifetimeMs, &application, [&] {
        application.exit(3);
    });
    output << u"PRIMARY\n"_s << Qt::flush;
    return application.exec();
}

int main(int argc, char **argv)
{
    auto application = QCoreApplication(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() == 4) {
        const auto &mode = arguments.at(1);
        const auto &identity = arguments.at(2);
        const auto &message = arguments.at(3);
        if (mode == u"--child") {
            auto guard = OCC::Utility::LinuxSingleApplication(identity, identity);
            return runChild(application, guard, message);
        }
        if (mode == u"--legacy-child") {
            auto guard = KDSingleApplication(identity);
            return runChild(application, guard, message);
        }
    }

    auto test = TestLinuxSingleApplication();
    return QTest::qExec(&test, argc, argv);
}

#include "testlinuxsingleapplication.moc"
