/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "advancedsettings.h"
#include "application.h"
#include "logger.h"

#include <KArchiveDirectory>
#include <KArchiveFile>
#include <KZip>

#include <QBuffer>
#include <QDir>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

class TestApplicationDiagnostics : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void archiveKeepsStartupOptionsAfterAnotherInvocation()
    {
        auto *const application = qobject_cast<OCC::Application *>(qApp);
        QVERIFY(application);
        QVERIFY(application->versionOnly());

        const auto closeLog = qScopeGuard([] {
            OCC::Logger::instance()->setLogFile({});
        });
        const auto forwardedOptions = QStringList{u"ApplicationDiagnosticsTest"_s, u"--debug"_s};
        QVERIFY(QMetaObject::invokeMethod(application, "slotActivateRequestedMessage", Qt::DirectConnection, forwardedOptions, QString{}));
        QVERIFY(application->debugMode());

        auto archiveData = QByteArray{};
        auto writeBuffer = QBuffer{&archiveData};
        auto writer = KZip{&writeBuffer};
        QVERIFY(writer.open(QIODevice::WriteOnly));
        OCC::AdvancedSettings::addDiagnosticCommandLineOptions(writer);
        QVERIFY(writer.close());

        auto readBuffer = QBuffer{&archiveData};
        auto reader = KZip{&readBuffer};
        QVERIFY2(reader.open(QIODevice::ReadOnly), qPrintable(reader.errorString()));
        const auto *parameters = reader.directory()->file(u"_client_parameters.txt"_s);
        QVERIFY(parameters);
        QCOMPARE(parameters->data(), "--background --logwindow --logflush"_ba);
    }
};

int main(int argc, char **argv)
{
    const auto temporaryDirectory = QTemporaryDir{};
    if (!temporaryDirectory.isValid()) {
        qCritical("Could not create test configuration directory.");
        return 1;
    }

#ifdef Q_OS_MACOS
    // The single-instance manager uses the process home before command-line parsing.
    if (!qputenv("HOME", temporaryDirectory.path().toLocal8Bit()) || QDir::homePath() != temporaryDirectory.path()) {
        qCritical("Could not isolate the test home directory.");
        return 1;
    }
#endif

    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationDomain(u"nextcloud.test"_s);
    QCoreApplication::setApplicationName(u"ApplicationDiagnosticsTest%1"_s.arg(QCoreApplication::applicationPid()));
    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(theme);

    auto arguments = std::array{
        QByteArray{argv[0]},
        "--version"_ba,
        "--background"_ba,
        "-l"_ba,
        "--logflush"_ba,
        "--apppassword=diagnostic-placeholder"_ba,
        "--confdir"_ba,
        temporaryDirectory.path().toLocal8Bit(),
        "--logfile"_ba,
        temporaryDirectory.filePath(u"client.log"_s).toLocal8Bit(),
    };
    auto applicationArgc = static_cast<int>(arguments.size());
    auto applicationArgv = std::array<char *, std::tuple_size_v<decltype(arguments)> + 1>{};
    std::ranges::transform(arguments, applicationArgv.begin(), [](auto &argument) {
        return argument.data();
    });
    auto application = OCC::Application{applicationArgc, applicationArgv.data()};
    auto test = TestApplicationDiagnostics{};
    return QTest::qExec(&test, argc, argv);
}

#include "testapplicationdiagnostics.moc"
