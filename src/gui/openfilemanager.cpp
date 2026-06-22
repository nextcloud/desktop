/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "openfilemanager.h"
#include "common/utility.h"
#include <QProcess>
#include <QSettings>
#include <QDir>
#include <QUrl>
#include <QDesktopServices>
#include <QApplication>
#include <QOperatingSystemVersion>

namespace OCC {

// according to the QStandardDir impl from Qt5
static QStringList xdgDataDirs()
{
    QStringList dirs;
    // http://standards.freedesktop.org/basedir-spec/latest/
    QString xdgDataDirsEnv = QFile::decodeName(qgetenv("XDG_DATA_DIRS"));
    if (xdgDataDirsEnv.isEmpty()) {
        dirs.append(QString::fromLatin1("/usr/local/share"));
        dirs.append(QString::fromLatin1("/usr/share"));
    } else {
        dirs = xdgDataDirsEnv.split(QLatin1Char(':'));
    }
    // local location
    QString xdgDataHome = QFile::decodeName(qgetenv("XDG_DATA_HOME"));
    if (xdgDataHome.isEmpty()) {
        xdgDataHome = QDir::homePath() + "/.local/share";
    }
    dirs.prepend(xdgDataHome);
    return dirs;
}

// Linux impl only, make sure to process %u and %U which might be returned
static QString findDefaultFileManager()
{
    QProcess p;
    p.start("xdg-mime", QStringList() << "query"
                                      << "default"
                                      << "inode/directory",
        QFile::ReadOnly);
    p.waitForFinished();
    QString fileName = QString::fromUtf8(p.readAll().trimmed());
    if (fileName.isEmpty())
        return QString();

    QFileInfo fi;
    const QStringList dirs = xdgDataDirs();
    const QStringList subdirs { "/applications/", "/applications/kde4/" };
    for (const auto &dir : dirs) {
        for (const auto &subdir : subdirs) {
            fi.setFile(dir + subdir + fileName);
            if (fi.exists()) {
                return fi.absoluteFilePath();
            }
        }
    }
    return QString();
}

// early dolphin versions did not have --select
static bool checkDolphinCanSelect()
{
    QProcess p;
    p.start("dolphin", QStringList() << "--help", QFile::ReadOnly);
    p.waitForFinished();
    return p.readAll().contains("--select");
}


// inspired by Qt Creator's showInGraphicalShell();
void showInFileManager(const QString &localPath)
{
    if (Utility::isWindows()) {
#ifdef Q_OS_WIN
            if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows7)
                return;
#endif

        const QString explorer = "explorer.exe "; // FIXME: we trust it's in PATH
        QFileInfo fi(localPath);

        // canonicalFilePath returns empty if the file does not exist
        if (!fi.canonicalFilePath().isEmpty()) {
            QString nativeArgs;
            if (!fi.isDir()) {
                nativeArgs += QLatin1String("/select,");
            }
            nativeArgs += QLatin1Char('"');
            nativeArgs += QDir::toNativeSeparators(fi.canonicalFilePath());
            nativeArgs += QLatin1Char('"');

            QProcess p;
#ifdef Q_OS_WIN
            // QProcess on Windows tries to wrap the whole argument/program string
            // with quotes if it detects a space in it, but explorer wants the quotes
            // only around the path. Use setNativeArguments to bypass this logic.
            p.setNativeArguments(nativeArgs);
#endif
            p.start(explorer, QStringList {});
            p.waitForFinished(5000);
        }
    } else if (Utility::isMac()) {
        QProcess::startDetached("/usr/bin/open", {"-R", localPath});
    } else {
        QString app;
        QStringList args;

        static QString defaultManager = findDefaultFileManager();
        QSettings desktopFile(defaultManager, QSettings::IniFormat);
        QString exec = desktopFile.value("Desktop Entry/Exec").toString();

        QString fileToOpen = QFileInfo(localPath).absoluteFilePath();
        QString pathToOpen = QFileInfo(localPath).absolutePath();
        bool canHandleFile = false; // assume dumb fm

        args = exec.split(' ');
        if (args.count() > 0)
            app = args.takeFirst();

        QString kdeSelectParam("--select");

        if (app.contains("konqueror") && !args.contains(kdeSelectParam)) {
            // konq needs '--select' in order not to launch the file
            args.prepend(kdeSelectParam);
            canHandleFile = true;
        }

        if (app.contains("dolphin")) {
            static bool dolphinCanSelect = checkDolphinCanSelect();
            if (dolphinCanSelect && !args.contains(kdeSelectParam)) {
                args.prepend(kdeSelectParam);
                canHandleFile = true;
            }
        }

        // whitelist
        if (app.contains("nautilus") || app.contains("nemo")) {
            canHandleFile = true;
        }

        static QString name;
        if (name.isEmpty()) {
            name = desktopFile.value(QString::fromLatin1("Desktop Entry/Name[%1]").arg(qApp->property("ui_lang").toString())).toString();
            if (name.isEmpty()) {
                name = desktopFile.value(QString::fromLatin1("Desktop Entry/Name")).toString();
            }
        }

        std::replace(args.begin(), args.end(), QString::fromLatin1("%c"), name);
        std::replace(args.begin(), args.end(), QString::fromLatin1("%u"), fileToOpen);
        std::replace(args.begin(), args.end(), QString::fromLatin1("%U"), fileToOpen);
        std::replace(args.begin(), args.end(), QString::fromLatin1("%f"), fileToOpen);
        std::replace(args.begin(), args.end(), QString::fromLatin1("%F"), fileToOpen);

        // fixme: needs to append --icon, according to http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables
        QStringList::iterator it = std::find(args.begin(), args.end(), QString::fromLatin1("%i"));
        if (it != args.end()) {
            (*it) = desktopFile.value("Desktop Entry/Icon").toString();
            args.insert(it, QString::fromLatin1("--icon")); // before
        }


        if (args.count() == 0)
            args << fileToOpen;

        if (app.isEmpty() || args.isEmpty() || !canHandleFile) {
            // fall back: open the default file manager, without ever selecting the file
            QDesktopServices::openUrl(QUrl::fromLocalFile(pathToOpen));
        } else {
            QProcess::startDetached(app, args);
        }
    }
}
}
