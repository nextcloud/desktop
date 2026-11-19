/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "application.h"
#include "configfile.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TestApplicationOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void configDirectoryIsStartupOnly()
    {
        QStandardPaths::setTestModeEnabled(true);
        const auto temporaryDir = QTemporaryDir(QDir::temp().filePath(u"nc-opt-XXXXXX"_s));
        QVERIFY(temporaryDir.isValid());
        qputenv("TMPDIR", QFile::encodeName(temporaryDir.path()));
        qputenv("XDG_RUNTIME_DIR", QFile::encodeName(temporaryDir.path()));

        const auto root = QDir(temporaryDir.path());
        const auto original = root.filePath(u"original|profile"_s);
        const auto split = root.filePath(u"split"_s);
        const auto inlineValue = root.filePath(u"inline"_s);
        const auto logPath = root.filePath(u"test.log"_s);
        // --version skips normal GUI and account setup.
        auto startup = QList<QByteArray>{"nextcloud", "--confdir", QFile::encodeName(original), "--logfile", QFile::encodeName(logPath), "--version"};
        QList<char *> argv;
        for (auto &argument : startup) {
            argv.append(argument.data());
        }
        argv.append(nullptr);
        auto argc = static_cast<int>(startup.size());
        auto application = OCC::Application(argc, argv.data());
        QVERIFY(application.versionOnly());
        QVERIFY(!application.isRunning());
        QCOMPARE(QDir::cleanPath(OCC::ConfigFile().configPath()), original);

        const auto forwardedOptions = QList<QStringList>{
            {u"--confdir"_s, split},
            {u"--confdir="_s + inlineValue},
            {u"--confdir="_s},
            {u"--confdir"_s, u"--debug"_s},
        };
        for (const auto &options : forwardedOptions) {
            const auto message = (u"MSG_PARSEOPTIONS:nextcloud|"_s + options.join(u'|')).toLatin1();
            QVERIFY(QMetaObject::invokeMethod(&application, "slotParseMessage", Qt::DirectConnection, Q_ARG(QByteArray, message)));
            QCOMPARE(QDir::cleanPath(OCC::ConfigFile().configPath()), original);
            QVERIFY(!QFile::exists(split));
            QVERIFY(!QFile::exists(inlineValue));
        }
        // Skipping the missing value must leave the next option available.
        QVERIFY(application.debugMode());

        // A secondary with an explicit profile must exit without creating it or forwarding --quit.
        const auto rejectedDirectory = root.filePath(u"rejected"_s);
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(u"XDG_CONFIG_HOME"_s, root.filePath(u"child-config"_s));
        environment.insert(u"XDG_DATA_HOME"_s, root.filePath(u"child-data"_s));
        auto secondary = QProcess();
        secondary.setProcessEnvironment(environment);
        secondary.start(QLatin1String(NEXTCLOUD_EXECUTABLE_PATH), {u"--confdir"_s, rejectedDirectory, u"--quit"_s});
        QVERIFY2(secondary.waitForFinished(10000), qPrintable(secondary.errorString()));
        QCOMPARE(secondary.exitStatus(), QProcess::NormalExit);
        QCOMPARE(secondary.exitCode(), 1);
        QVERIFY(secondary.readAllStandardError().contains("Quit the running desktop client before using --confdir"));
        QVERIFY(!QFile::exists(rejectedDirectory));
    }
};

QTEST_APPLESS_MAIN(TestApplicationOptions)
#include "testapplicationoptions.moc"
