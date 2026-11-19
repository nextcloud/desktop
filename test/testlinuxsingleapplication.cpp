/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDeadlineTimer>
#include <QProcess>
#include <QProcessEnvironment>
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

    QProcess *startChild(const QString &name, const QString &session, const QString &message = {}, const QString &profile = {})
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
        const auto configDirectory = profile.isEmpty() ? _directory->filePath(u"default"_s) : profile;
        process->start(QCoreApplication::applicationFilePath(), {u"--child"_s, name, configDirectory, message});
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
        auto primary = startChild(identity, firstSession);
        QCOMPARE(readLine(primary), QByteArray("PRIMARY"));
        const auto message = u"forwarded action"_s;
        auto secondary = startChild(identity, secondSession, message);
        QCOMPARE(readLine(secondary), QByteArray("SECONDARY"));
        QVERIFY(secondary->state() == QProcess::NotRunning || secondary->waitForFinished(processTimeoutMs));
        QCOMPARE(secondary->exitStatus(), QProcess::NormalExit);
        QCOMPARE(secondary->exitCode(), 0);
        QCOMPARE(readLine(primary), QByteArray("MESSAGE ") + message.toUtf8().toHex());
        QCOMPARE(primary->state(), QProcess::Running);
    }

    void distinctProfiles()
    {
        const auto identity = u"nextcloud-profile-test"_s;
        const auto session = u"session-1"_s;
        auto first = startChild(identity, session, {}, _directory->filePath(u"profile-a"_s));
        auto second = startChild(identity, session, {}, _directory->filePath(u"profile-b"_s));
        QCOMPARE(readLine(first), QByteArray("PRIMARY"));
        QCOMPARE(readLine(second), QByteArray("PRIMARY"));
        stopChild(first);
        stopChild(second);
    }

    void sameProfileThroughAlias()
    {
        const auto identity = u"nextcloud-profile-test"_s;
        const auto configuration = _directory->filePath(u"profile"_s);
        QVERIFY(QDir().mkpath(configuration));
        const auto alias = _directory->filePath(u"alias"_s);
        QVERIFY(QFile::link(configuration, alias));

        auto primary = startChild(identity, u"session-1"_s, {}, configuration);
        QCOMPARE(readLine(primary), QByteArray("PRIMARY"));
        const auto message = u"aliased profile"_s;
        auto secondary = startChild(identity, u"session-2"_s, message, alias);
        QCOMPARE(readLine(secondary), QByteArray("SECONDARY"));
        QCOMPARE(readLine(primary), QByteArray("MESSAGE ") + message.toUtf8().toHex());
        stopChild(primary);
    }

    void profileIdentityBeforeDirectoryExists()
    {
        using OCC::Utility::linuxSingleApplicationName;
        const auto identity = u"nextcloud-profile-test"_s;
        const auto directory = _directory->filePath(u"real"_s);
        const auto alias = _directory->filePath(u"alias"_s);
        QVERIFY(QDir().mkpath(directory));
        QVERIFY(QFile::link(directory, alias));

        const auto configuration = QDir(directory).filePath(u"new/profile"_s);
        const auto aliasedConfiguration = QDir(alias).filePath(u"new/profile"_s);
        const auto relativeConfiguration = QDir::current().relativeFilePath(configuration);
        const auto key = linuxSingleApplicationName(identity, configuration);
        QCOMPARE(linuxSingleApplicationName(identity, aliasedConfiguration), key);
        QCOMPARE(linuxSingleApplicationName(identity, relativeConfiguration), key);
        QVERIFY(linuxSingleApplicationName(identity, directory) != key);
        QVERIFY(linuxSingleApplicationName(u"other-brand"_s, configuration) != key);
        QVERIFY(!QFile::exists(configuration));
        QVERIFY(QDir().mkpath(configuration));
        QCOMPARE(linuxSingleApplicationName(identity, aliasedConfiguration), key);
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

int main(int argc, char **argv)
{
    auto application = QCoreApplication(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() == 5 && arguments.at(1) == u"--child") {
        constexpr auto messageTimeoutMs = 1000;
        constexpr auto childLifetimeMs = 15000;
        const auto instanceName = OCC::Utility::linuxSingleApplicationName(arguments.at(2), arguments.at(3));
        auto guard = KDSingleApplication(instanceName, KDSingleApplication::Option::IncludeUsernameInSocketName);
        auto output = QTextStream(stdout);
        if (!guard.isPrimaryInstance()) {
            if (!guard.sendMessageWithTimeout(arguments.at(4).toUtf8(), messageTimeoutMs)) {
                return 2;
            }
            output << u"SECONDARY\n"_s << Qt::flush;
            return 0;
        }
        QObject::connect(&guard, &KDSingleApplication::messageReceived, &application, [&](const auto &message) {
            output << u"MESSAGE "_s << QString::fromLatin1(message.toHex()) << Qt::endl;
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

    auto test = TestLinuxSingleApplication();
    return QTest::qExec(&test, argc, argv);
}

#include "testlinuxsingleapplication.moc"
