/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QWidget>
#include <QDebug>

#ifdef Q_OS_UNIX
#include <sys/statvfs.h>
#include <sys/types.h>
#endif

#include <stdarg.h>

#if defined(Q_OS_MAC)
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#elif defined(Q_OS_WIN)
#include <shlobj.h>
#include <winbase.h>
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

    QString fp = QString::fromAscii( hash.trimmed() );
    fp.replace(QChar(' '), QChar(':'));

    return fp;
}

void Utility::setupFavLink(const QString &folder)
{
#ifdef Q_OS_WIN
    // Windows Explorer: Place under "Favorites" (Links)
    wchar_t path[MAX_PATH];
    SHGetSpecialFolderPath(0, path, CSIDL_PROFILE, FALSE);
    QString profile =  QDir::fromNativeSeparators(QString::fromWCharArray(path));
    QDir folderDir(QDir::fromNativeSeparators(folder));
    QString linkName = profile+QLatin1String("/Links/") + folderDir.dirName() + QLatin1String(".lnk");
    if (!QFile::link(folder, linkName))
        qDebug() << Q_FUNC_INFO << "linking" << folder << "to" << linkName << "failed!";
#elif defined (Q_OS_MAC)
    // Finder: Place under "Places"/"Favorites" on the left sidebar
    CFStringRef folderCFStr = CFStringCreateWithCString(0, folder.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath (0, folderCFStr, kCFURLPOSIXPathStyle, true);

    LSSharedFileListRef placesItems = LSSharedFileListCreate(0, kLSSharedFileListFavoriteItems, 0);
    if (placesItems) {
        //Insert an item to the list.
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(placesItems,
                                                                     kLSSharedFileListItemLast, 0, 0,
                                                                     urlRef, 0, 0);
        if (item)
            CFRelease(item);
    }
    CFRelease(placesItems);
    CFRelease(folderCFStr);
    CFRelease(urlRef);
#elif defined (Q_OS_UNIX)
    // Nautilus: add to ~/.gtk-bookmarks
    QFile gtkBookmarks(QDir::homePath()+QLatin1String("/.gtk-bookmarks"));
    QByteArray folderUrl = "file://" + folder.toUtf8();
    if (gtkBookmarks.open(QFile::ReadWrite)) {
        QByteArray places = gtkBookmarks.readAll();
        if (!places.contains(folderUrl)) {
            places += folderUrl;
            gtkBookmarks.reset();
            gtkBookmarks.write(places + '\n');
        }
    }

#endif
}

QString Utility::octetsToString( qint64 octets )
{
    static const qint64 kb = 1024;
    static const qint64 mb = 1024 * kb;
    static const qint64 gb = 1024 * mb;
    static const qint64 tb = 1024 * gb;

    if (octets >= tb) {
        if (octets < 10*tb) {
            return compactFormatDouble(qreal(octets)/qreal(tb), 1, QLatin1String("TB"));
        }
        return QString::number(qRound64(qreal(octets)/qreal(tb))) + QLatin1String(" TB");
    } else if (octets >= gb) {
        if (octets < 10*gb) {
            return compactFormatDouble(qreal(octets)/qreal(gb), 1, QLatin1String("GB"));
        }
        return QString::number(qRound64(qreal(octets)/qreal(gb))) + QLatin1String(" GB");
    } else if (octets >= mb) {
        if (octets < 10*mb) {
            return compactFormatDouble(qreal(octets)/qreal(mb), 1, QLatin1String("MB"));
        }
        return QString::number(qRound64(qreal(octets)/qreal(mb))) + QLatin1String(" MB");
    } else if (octets >= kb) {
        return QString::number(qRound64(qreal(octets)/qreal(kb))) + QLatin1String(" KB");
    } else if (octets == 1){
        return QLatin1String("1 byte");
    } else {
        return QString::number(octets) + QLatin1String(" bytes");
    }
}

// Qtified version of get_platforms() in csync_owncloud.c
QString Utility::platform()
{
#if defined(Q_OS_WIN32)
    return QLatin1String("Windows");
#elif defined(Q_OS_MAC)
    return QLatin1String("Macintosh");
#elif defined(Q_OS_LINUX)
    return QLatin1String("Linux");
#elif defined(__DragonFly__) // Q_OS_FREEBSD also defined
    return "DragonFlyBSD";
#elif defined(Q_OS_FREEBSD)
    return QLatin1String("FreeBSD");
#elif defined(Q_OS_NETBSD)
    return QLatin1String("NetBSD");
#elif defined(Q_OS_OPENBSD)
    return QLatin1String("OpenBSD");
#elif defined(Q_OS_SOLARIS)
    return "Solaris";
#else
    return "Unknown OS"
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
#if defined(Q_WS_WIN) || defined (Q_OS_MAC)
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

static const char runPathC[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool Utility::hasLaunchOnStartup(const QString &appName)
{
#if defined(Q_OS_WIN)
    QString runPath = QLatin1String(runPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    return settings.contains(appName);
#elif defined(Q_OS_MAC)
    // this is quite some duplicate code with setLaunchOnStartup, at some point we should fix this FIXME.
    bool returnValue = false;
    QString filePath = QDir(QCoreApplication::applicationDirPath()+QLatin1String("/../..")).absolutePath();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath (0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);
    if (loginItems) {
        // We need to iterate over the items and check which one is "ours".
        UInt32 seedValue;
        CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
        CFStringRef appUrlRefString = CFURLGetString(urlRef); // no need for release
        for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(itemsArray, i);
            CFURLRef itemUrlRef = NULL;

            if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr) {
                CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
                if (CFStringCompare(itemUrlString,appUrlRefString,0) == kCFCompareEqualTo) {
                    returnValue = true;
                }
                CFRelease(itemUrlRef);
            }
        }
        CFRelease(itemsArray);
    }
    CFRelease(loginItems);
    CFRelease(folderCFStr);
    CFRelease(urlRef);
    return returnValue;
#elif defined(Q_OS_UNIX)
    QString userAutoStartPath = QDir::homePath()+QLatin1String("/.config/autostart/");
    QString desktopFileLocation = userAutoStartPath+appName+QLatin1String(".desktop");
    return QFile::exists(desktopFileLocation);
#endif
}

namespace {
}

void Utility::setLaunchOnStartup(const QString &appName, const QString& guiName, bool enable)
{
#if defined(Q_OS_WIN)
    Q_UNUSED(guiName)
    QString runPath = QLatin1String(runPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    if (enable) {
        settings.setValue(appName, QCoreApplication::applicationFilePath().replace('/','\\'));
    } else {
        settings.remove(appName);
    }
#elif defined(Q_OS_MAC)
    Q_UNUSED(guiName)
    QString filePath = QDir(QCoreApplication::applicationDirPath()+QLatin1String("/../..")).absolutePath();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath (0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);

    if (loginItems && enable) {
        //Insert an item to the list.
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(loginItems,
                                                                     kLSSharedFileListItemLast, 0, 0,
                                                                     urlRef, 0, 0);
        if (item)
            CFRelease(item);
        CFRelease(loginItems);
    } else if (loginItems && !enable){
        // We need to iterate over the items and check which one is "ours".
        UInt32 seedValue;
        CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
        CFStringRef appUrlRefString = CFURLGetString(urlRef);
        for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(itemsArray, i);
            CFURLRef itemUrlRef = NULL;

            if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr) {
                CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
                if (CFStringCompare(itemUrlString,appUrlRefString,0) == kCFCompareEqualTo) {
                    LSSharedFileListItemRemove(loginItems,item); // remove it!
                }
                CFRelease(itemUrlRef);
            }
        }
        CFRelease(itemsArray);
        CFRelease(loginItems);
    };

    CFRelease(folderCFStr);
    CFRelease(urlRef);
#elif defined(Q_OS_UNIX)
    QString userAutoStartPath = QDir::homePath()+QLatin1String("/.config/autostart/");
    QString desktopFileLocation = userAutoStartPath+appName+QLatin1String(".desktop");
    if (enable) {
        if (!QDir().exists(userAutoStartPath) && !QDir().mkdir(userAutoStartPath)) {
            qDebug() << "Could not create autostart directory";
            return;
        }
        QFile iniFile(desktopFileLocation);
        if (!iniFile.open(QIODevice::WriteOnly)) {
            qDebug() << "Could not write auto start entry" << desktopFileLocation;
            return;
        }
        QTextStream ts(&iniFile);
        ts.setCodec("UTF-8");
        ts << QLatin1String("[Desktop Entry]") << endl
           << QLatin1String("Name=") << guiName << endl
           << QLatin1String("GenericName=") << QLatin1String("File Synchronizer") << endl
           << QLatin1String("Exec=") << QCoreApplication::applicationFilePath() << endl
           << QLatin1String("Terminal=") << "false" << endl
           << QLatin1String("Icon=") << appName << endl
           << QLatin1String("Categories=") << QLatin1String("Network") << endl
           << QLatin1String("Type=") << QLatin1String("Application") << endl
           << QLatin1String("StartupNotify=") << "false" << endl
           << QLatin1String("X-GNOME-Autostart-enabled=") << "true" << endl
            ;
    } else {
        if (!QFile::remove(desktopFileLocation)) {
            qDebug() << "Could not remove autostart desktop file";
        }
    }

#endif
}

qint64 Utility::freeDiskSpace(const QString &path, bool *ok)
{
#ifdef Q_OS_MAC
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

} // namespace Mirall
