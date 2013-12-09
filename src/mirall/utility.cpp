/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "utility.h"

#include "mirall/version.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QWidget>
#include <QDebug>
#include <QDesktopServices>
#include <QProcess>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QDesktopServices>
#include <QTextDocument>
#else
#include <QStandardPaths>
#endif

#ifdef Q_OS_UNIX
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <stdarg.h>

#if defined(Q_OS_WIN)
#include "mirall/utility_win.cpp"
#elif defined(Q_OS_MAC)
#include "mirall/utility_mac.cpp"
#else
#include "mirall/utility_unix.cpp"
#endif

namespace Mirall {

QString Utility::formatFingerprint( const QByteArray& fmhash )
{
    QByteArray hash;
    int steps = fmhash.length()/2;
    for (int i = 0; i < steps; i++) {
        hash.append(fmhash[i*2]);
        hash.append(fmhash[i*2+1]);
        hash.append(' ');
    }

    QString fp = QString::fromLatin1( hash.trimmed() );
    fp.replace(QChar(' '), QChar(':'));

    return fp;
}

void Utility::setupFavLink(const QString &folder)
{
    setupFavLink_private(folder);
}

QString Utility::octetsToString( qint64 octets )
{
    static const qint64 kb = 1024;
    static const qint64 mb = 1024 * kb;
    static const qint64 gb = 1024 * mb;
    static const qint64 tb = 1024 * gb;

    QString s;
    qreal value = octets;
    if (octets >= tb) {
        s = QCoreApplication::translate("Utility", "%L1 TB");
        value /= tb;
    } else if (octets >= gb) {
        s = QCoreApplication::translate("Utility", "%L1 GB");
        value /= gb;
    } else if (octets >= mb) {
        s = QCoreApplication::translate("Utility", "%L1 MB");
        value /= mb;
    } else if (octets >= kb) {
        s = QCoreApplication::translate("Utility", "%L1 kB");
        value /= kb;
    } else  {
        s = QCoreApplication::translate("Utility", "%L1 B");
    }

    return (value > 9.95)  ? s.arg(qRound(value)) : s.arg(value, 0, 'g', 2);
}

// Qtified version of get_platforms() in csync_owncloud.c
QString Utility::platform()
{
#if defined(Q_OS_WIN)
    return QLatin1String("Windows");
#elif defined(Q_OS_MAC)
    return QLatin1String("Macintosh");
#elif defined(Q_OS_LINUX)
    return QLatin1String("Linux");
#elif defined(__DragonFly__) // Q_OS_FREEBSD also defined
    return QLatin1String("DragonFlyBSD");
#elif defined(Q_OS_FREEBSD)
    return QLatin1String("FreeBSD");
#elif defined(Q_OS_NETBSD)
    return QLatin1String("NetBSD");
#elif defined(Q_OS_OPENBSD)
    return QLatin1String("OpenBSD");
#elif defined(Q_OS_SOLARIS)
    return QLatin1String("Solaris");
#else
    return QLatin1String("Unknown OS");
#endif
}

QByteArray Utility::userAgentString()
{
    return QString::fromLatin1("Mozilla/5.0 (%1) mirall/%2")
            .arg(Utility::platform())
            .arg(QLatin1String(MIRALL_STRINGIFY(MIRALL_VERSION)))
            .toLatin1();
}

void Utility::raiseDialog( QWidget *raiseWidget )
{
    // viel hilft viel ;-)
    if( raiseWidget ) {
#if defined(Q_OS_WIN) || defined (Q_OS_MAC)
        Qt::WindowFlags eFlags = raiseWidget->windowFlags();
        eFlags |= Qt::WindowStaysOnTopHint;
        raiseWidget->setWindowFlags(eFlags);
        raiseWidget->show();
        eFlags &= ~Qt::WindowStaysOnTopHint;
        raiseWidget->setWindowFlags(eFlags);
#endif
        raiseWidget->show();
        raiseWidget->raise();
        raiseWidget->activateWindow();
    }
}

bool Utility::hasLaunchOnStartup(const QString &appName)
{
    return hasLaunchOnStartup_private(appName);
}

void Utility::setLaunchOnStartup(const QString &appName, const QString& guiName, bool enable)
{
    setLaunchOnStartup_private(appName, guiName, enable);
}

qint64 Utility::freeDiskSpace(const QString &path, bool *ok)
{
#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD)
    struct statvfs stat;
    statvfs(path.toUtf8().data(), &stat);
    return (qint64) stat.f_bavail * stat.f_frsize;
#elif defined(Q_OS_UNIX)
    Q_UNUSED(ok)
    struct statvfs64 stat;
    statvfs64(path.toUtf8().data(), &stat);
    return (qint64) stat.f_bavail * stat.f_frsize;
#elif defined(Q_OS_WIN)
    ULARGE_INTEGER freeBytes;
    freeBytes.QuadPart = 0L;
    QString drive = QDir().absoluteFilePath(path).left(2);
    if( !GetDiskFreeSpaceEx( reinterpret_cast<const wchar_t *>(drive.utf16()), &freeBytes, NULL, NULL ) ) {
        if (ok) *ok = false;
    }
    return freeBytes.QuadPart;
#else
    if (ok) *ok = false;
    return 0;
#endif
}

QString Utility::compactFormatDouble(double value, int prec, const QString& unit)
{
    QLocale locale = QLocale::system();
    QChar decPoint = locale.decimalPoint();
    QString str = locale.toString(value, 'f', prec);
    while (str.endsWith('0') || str.endsWith(decPoint)) {
        if (str.endsWith(decPoint)) {
            str.chop(1);
            break;
        }
        str.chop(1);
    }
    if( !unit.isEmpty() )
        str += (QLatin1Char(' ')+unit);
    return str;
}

QString Utility::toCSyncScheme(const QString &urlStr)
{

    QUrl url( urlStr );
    if( url.scheme() == QLatin1String("http") ) {
        url.setScheme( QLatin1String("owncloud") );
    } else {
        // connect SSL!
        url.setScheme( QLatin1String("ownclouds") );
    }
    return url.toString();
}

QString Utility::escape(const QString &in)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    return Qt::escape(in);
#else
    return in.toHtmlEscaped();
#endif
}

QString Utility::dataLocation()
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#endif
}

void Utility::sleep(int sec)
{
#ifdef Q_OS_WIN
    ::Sleep(sec*1000);
#else
    ::sleep(sec);
#endif
}

// ### helper functions for showInFileManager() ###

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
        xdgDataHome = QDir::homePath()+"/.local/share";
    }
    dirs.prepend(xdgDataHome);
    return dirs;
}

// Linux impl only, make sure to process %u and %U which might be returned
static QString findDefaultFileManager()
{
    QProcess p;
    p.start("xdg-mime", QStringList() << "query" << "default" << "inode/directory", QFile::ReadOnly);
    p.waitForFinished();
    QString fileName = QString::fromUtf8(p.readAll().trimmed());
    if (fileName.isEmpty())
        return QString();

    QFileInfo fi;
    QStringList dirs = xdgDataDirs();
    QStringList subdirs;
    subdirs << "/applications/" << "/applications/kde4/";
    foreach(QString dir, dirs) {
        foreach(QString subdir, subdirs) {
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
void Utility::showInFileManager(const QString &localPath)
{
    if (isWindows()) {
        const QString explorer = "explorer.exe"; // FIXME: we trust it's in PATH
        QString param;
        if (!QFileInfo(localPath).isDir())
            param += QLatin1String("/select,");
        param += QDir::toNativeSeparators(localPath);
        QProcess::startDetached(explorer, QStringList(param));
    } else if (isMac()) {
        QStringList scriptArgs;
        scriptArgs << QLatin1String("-e")
                   << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
                      .arg(localPath);
        QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
        scriptArgs.clear();
        scriptArgs << QLatin1String("-e")
                   << QLatin1String("tell application \"Finder\" to activate");
        QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
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
        if (args.count() > 0) app = args.takeFirst();

        QString kdeSelectParam("--select");

        if (app.contains("konqueror") && !args.contains(kdeSelectParam)) {
            // konq needs '--select' in order not to launch the file
            args.prepend(kdeSelectParam);
            canHandleFile = true;
        }

        if (app.contains("dolphin"))
        {
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


        if (args.count() == 0) args << fileToOpen;

        if (app.isEmpty() || args.isEmpty() || !canHandleFile) {
            // fall back: open the default file manager, without ever selecting the file
            QDesktopServices::openUrl(QUrl::fromLocalFile(pathToOpen));
        } else {
            QProcess::execute(app, args);
        }
    }
}

bool Utility::isWindows()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool Utility::isMac()
{
#ifdef Q_OS_MAC
    return true;
#else
    return false;
#endif
}

bool Utility::isUnix()
{
#ifdef Q_OS_UNIX
    return true;
#else
    return false;
#endif
}


} // namespace Mirall
