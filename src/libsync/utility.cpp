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

#include "version.h"
#include "config.h"

// Note:  This file must compile without QtGui
#include <QCoreApplication>
#include <QSettings>
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QDebug>
#include <QProcess>
#include <QThread>
#include <QDateTime>
#include <QSysInfo>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
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
#include "utility_win.cpp"
#elif defined(Q_OS_MAC)
#include "utility_mac.cpp"
#else
#include "utility_unix.cpp"
#endif

namespace OCC {

bool Utility::writeRandomFile( const QString& fname, int size )
{
    int maxSize = 10*10*1024;
    qsrand(QDateTime::currentMSecsSinceEpoch());

    if( size == -1 ) size = qrand() % maxSize;

    QString randString;
    for( int i = 0; i < size; i++ ) {
        int r = qrand() % 128;
        randString.append(QChar(r));
    }

    QFile file(fname);
    if( file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
        QTextStream out(&file);
        out << randString;
     // optional, as QFile destructor will already do it:
        file.close();
        return true;
    }
    return false;

}

QString Utility::formatFingerprint( const QByteArray& fmhash, bool colonSeparated )
{
    QByteArray hash;
    int steps = fmhash.length()/2;
    for (int i = 0; i < steps; i++) {
        hash.append(fmhash[i*2]);
        hash.append(fmhash[i*2+1]);
        hash.append(' ');
    }

    QString fp = QString::fromLatin1( hash.trimmed() );
    if (colonSeparated) {
        fp.replace(QChar(' '), QChar(':'));
    }

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
        s = QCoreApplication::translate("Utility", "%L1 TiB");
        value /= tb;
    } else if (octets >= gb) {
        s = QCoreApplication::translate("Utility", "%L1 GiB");
        value /= gb;
    } else if (octets >= mb) {
        s = QCoreApplication::translate("Utility", "%L1 MiB");
        value /= mb;
    } else if (octets >= kb) {
        s = QCoreApplication::translate("Utility", "%L1 KiB");
        value /= kb;
    } else  {
        s = QCoreApplication::translate("Utility", "%L1 B");
    }

    return (value > 9.95)  ? s.arg(qRound(value)) : s.arg(value, 0, 'f', 2);
}

// Qtified version of get_platforms() in csync_owncloud.c
static QLatin1String platform()
{
#if defined(Q_OS_WIN)
    return QLatin1String("Windows");
#elif defined(Q_OS_MAC)
    return QLatin1String("Macintosh");
#elif defined(Q_OS_LINUX)
    return QLatin1String("Linux");
#elif defined(__DragonFly__) // Q_OS_FREEBSD also defined
    return QLatin1String("DragonFlyBSD");
#elif defined(Q_OS_FREEBSD) || defined(Q_OS_FREEBSD_KERNEL)
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
    QString re = QString::fromLatin1("Mozilla/5.0 (%1) mirall/%2")
            .arg(platform())
            .arg(QLatin1String(MIRALL_STRINGIFY(MIRALL_VERSION)));

    QLatin1String appName(APPLICATION_SHORTNAME);

    // this constant "ownCloud" is defined in the default OEM theming
    // that is used for the standard client. If it is changed there,
    // it needs to be adjusted here.
    if( appName != QLatin1String("ownCloud") ) {
        re += QString(" (%1)").arg(appName);
    }
    return re.toLatin1();
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
#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD) || defined(Q_OS_FREEBSD_KERNEL) || defined(Q_OS_NETBSD)
    Q_UNUSED(ok)
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
    if( !GetDiskFreeSpaceEx( reinterpret_cast<const wchar_t *>(path.utf16()), &freeBytes, NULL, NULL ) ) {
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

bool Utility::doesSetContainPrefix(const QSet<QString> &l, const QString &p) {

    Q_FOREACH (const QString &setPath, l) {
        //qDebug() << Q_FUNC_INFO << p << setPath << setPath.startsWith(p);
        if (setPath.startsWith(p)) {
            return true;
        }
    }
    //qDebug() << "-> NOOOOO!!!" << p << l.count();
    return false;
}

QString Utility::escape(const QString &in)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    return Qt::escape(in);
#else
    return in.toHtmlEscaped();
#endif
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
// In Qt 4,  QThread::sleep functions are protected.
// This is a hack to make them visible in this namespace.
struct QThread : ::QThread {
    using ::QThread::sleep;
    using ::QThread::usleep;
};
#endif

void Utility::sleep(int sec)
{
    QThread::sleep(sec);
}

void Utility::usleep(int usec)
{
    QThread::usleep(usec);
}

bool Utility::fsCasePreserving()
{
    bool re = false;
    if( isWindows() || isMac() ) {
        re = true;
    } else {
        static bool isTest = qgetenv("OWNCLOUD_TEST_CASE_PRESERVING").toInt();
        re = isTest;
    }
    return re;
}

QDateTime Utility::qDateTimeFromTime_t(qint64 t)
{
    return QDateTime::fromMSecsSinceEpoch(t * 1000);
}

qint64 Utility::qDateTimeToTime_t(const QDateTime& t)
{
    return t.toMSecsSinceEpoch() / 1000;
}

QString Utility::durationToDescriptiveString(quint64 msecs)
{
    struct Period { const char *name; quint64 msec; };
    Q_DECL_CONSTEXPR Period periods[] = {
        { QT_TRANSLATE_NOOP("Utility", "%Ln year(s)") , 365*24*3600*1000L },
        { QT_TRANSLATE_NOOP("Utility", "%Ln month(s)") , 30*24*3600*1000L },
        { QT_TRANSLATE_NOOP("Utility", "%Ln day(s)") , 24*3600*1000L },
        { QT_TRANSLATE_NOOP("Utility", "%Ln hour(s)") , 3600*1000L },
        { QT_TRANSLATE_NOOP("Utility", "%Ln minute(s)") , 60*1000L },
        { QT_TRANSLATE_NOOP("Utility", "%Ln second(s)") , 1000L },
        { 0, 0 }
    };

    int p = 0;
    while (periods[p].name && msecs < periods[p].msec) {
        p++;
    }

    if (!periods[p].name) {
        return QCoreApplication::translate("Utility", "0 seconds");
    }

    auto firstPart = QCoreApplication::translate("Utility", periods[p].name, 0, QCoreApplication::UnicodeUTF8, int(msecs / periods[p].msec));

    if (!periods[p+1].name) {
        return firstPart;
    }

    quint64 secondPartNum = qRound( double(msecs % periods[p].msec) / periods[p+1].msec);

    if (secondPartNum == 0) {
        return firstPart;
    }

    return QCoreApplication::translate("Utility", "%1 %2").arg(firstPart,
            QCoreApplication::translate("Utility", periods[p+1].name, 0, QCoreApplication::UnicodeUTF8, secondPartNum));
}

bool Utility::hasDarkSystray()
{
    return hasDarkSystray_private();
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

bool Utility::isLinux()
{
#if defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

bool Utility::isBSD()
{
#if defined(Q_OS_FREEBSD) || defined(Q_OS_NETBSD)
    return true;
#else
    return false;
#endif
}


void Utility::crash()
{
    volatile int* a = (int*)(NULL);
    *a = 1;
}

static const char STOPWATCH_END_TAG[] = "_STOPWATCH_END";

void Utility::StopWatch::start()
{
    _startTime = QDateTime::currentDateTime();
    _timer.start();
}

quint64 Utility::StopWatch::stop()
{
    addLapTime(QLatin1String(STOPWATCH_END_TAG));
    quint64 duration = _timer.elapsed();
    _timer.invalidate();
    return duration;
}

void Utility::StopWatch::reset()
{
    _timer.invalidate();
    _startTime.setMSecsSinceEpoch(0);
    _lapTimes.clear();
}

quint64 Utility::StopWatch::addLapTime( const QString& lapName )
{
    if( !_timer.isValid() ) {
        start();
    }
    quint64 re = _timer.elapsed();
    _lapTimes[lapName] = re;
    return re;
}

QDateTime Utility::StopWatch::startTime() const
{
    return _startTime;
}

QDateTime Utility::StopWatch::timeOfLap( const QString& lapName ) const
{
    quint64 t = durationOfLap(lapName);
    if( t ) {
        QDateTime re(_startTime);
        return re.addMSecs(t);
    }

    return QDateTime();
}

quint64 Utility::StopWatch::durationOfLap( const QString& lapName ) const
{
    return _lapTimes.value(lapName, 0);
}

} // namespace OCC
