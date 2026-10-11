/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "common/utility.h"
#include "folderwatcher.h"

#include <QProcess>
#include <QtTest>

using namespace OCC;

class TestFolderWatcherChangeDetection : public QObject
{
    Q_OBJECT

    QScopedPointer<QTemporaryDir> _root;
    QString _rootPath;
    QScopedPointer<FolderWatcher> _watcher;
    QString _junction; //!< removed in cleanup(), the temporary directory removal must not follow it

    QStringList reportedPathsFor(const QString &path)
    {
        auto pathChangedSpy = QSignalSpy(_watcher.data(), &FolderWatcher::pathChanged);
        if (!QMetaObject::invokeMethod(_watcher.data(), "changeDetected", Qt::DirectConnection, Q_ARG(QString, path))) {
            return {};
        }

        auto paths = QStringList{};
        for (const auto &args : std::as_const(pathChangedSpy)) {
            paths.append(args.first().toString());
        }
        return paths;
    }

private Q_SLOTS:
    void init()
    {
        _root.reset(new QTemporaryDir);
        QVERIFY(_root->isValid());
        _rootPath = QDir(_root->path()).canonicalPath();
        _watcher.reset(new FolderWatcher);
        _watcher->init(_rootPath);
    }

    void cleanup()
    {
        _watcher.reset();
        if (!_junction.isEmpty()) {
            QDir().rmdir(_junction);
            _junction.clear();
        }
        _root.reset();
    }

    void testChangeDetected_directory_reportsItsContent()
    {
        const auto dir = QString{_rootPath + "/dir"};
        QVERIFY(QDir().mkpath(dir + "/sub"));
        Utility::writeRandomFile(dir + "/sub/file.bin");

        const auto paths = reportedPathsFor(dir);
        QVERIFY(paths.contains(dir));
        QVERIFY(paths.contains(dir + "/sub"));
        QVERIFY(paths.contains(dir + "/sub/file.bin"));
    }

    void testChangeDetected_directoryWithSymlinkToParent_symlinkNotTraversed()
    {
#ifdef Q_OS_WIN
        QSKIP("Creating symlinks requires extra privileges on Windows");
#endif
        const auto dir = QString{_rootPath + "/dir"};
        QVERIFY(QDir().mkpath(dir + "/sub"));
        QVERIFY(QFile::link(QStringLiteral(".."), dir + "/sub/loop"));
        QVERIFY(QFileInfo(dir + "/sub/loop").isDir());

        const auto paths = reportedPathsFor(dir);
        QVERIFY(paths.contains(dir + "/sub/loop"));
        for (const auto &path : paths) {
            QVERIFY2(!path.startsWith(dir + "/sub/loop/"), qPrintable(QStringLiteral("descended into symlink: %1").arg(path)));
        }
    }

    void testChangeDetected_symlinkToDirectory_onlyReportsTheSymlink()
    {
#ifdef Q_OS_WIN
        QSKIP("Creating symlinks requires extra privileges on Windows");
#endif
        const auto target = QString{_rootPath + "/target"};
        QVERIFY(QDir().mkpath(target + "/sub"));
        const auto link = QString{_rootPath + "/link"};
        QVERIFY(QFile::link(target, link));

        QCOMPARE(reportedPathsFor(link), QStringList{link});
    }

    void testChangeDetected_directoryWithJunctionToParent_junctionNotTraversed()
    {
#ifndef Q_OS_WIN
        QSKIP("Junctions only exist on Windows");
#endif
        const auto dir = QString{_rootPath + "/dir"};
        QVERIFY(QDir().mkpath(dir + "/sub"));
        _junction = dir + "/sub/junction";
        const auto mklinkArguments = QStringList{QStringLiteral("/c"),
                                                 QStringLiteral("mklink"),
                                                 QStringLiteral("/J"),
                                                 QDir::toNativeSeparators(_junction),
                                                 QDir::toNativeSeparators(dir)};
        QCOMPARE(QProcess::execute(QStringLiteral("cmd.exe"), mklinkArguments), 0);
        QVERIFY(QFileInfo(_junction).isJunction());

        const auto paths = reportedPathsFor(dir);
        QVERIFY(paths.contains(_junction));
        for (const auto &path : paths) {
            QVERIFY2(!path.startsWith(_junction + "/"), qPrintable(QStringLiteral("descended into junction: %1").arg(path)));
        }
        QCOMPARE(reportedPathsFor(_junction), QStringList{_junction});
    }
};

QTEST_GUILESS_MAIN(TestFolderWatcherChangeDetection)
#include "testfolderwatcherchangedetection.moc"
