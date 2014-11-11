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
    return QString::fromLatin1("Mozilla/5.0 (%1) mirall/%2")
            .arg(Utility::platform())
            .arg(QLatin1String(MIRALL_STRINGIFY(MIRALL_VERSION)))
            .toLatin1();
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
#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD) || defined(Q_OS_FREEBSD_KERNEL)
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

//TODO change to initializers list  when possible.
static QList<QPair<QString,quint32> > timeMapping = QList<QPair<QString,quint32> >() <<
                                                    QPair<QString,quint32>("%1 years",86400*365) <<
                                                    QPair<QString,quint32>("%1 months",86400*30) <<
                                                    QPair<QString,quint32>("%1 days",86400) <<
                                                    QPair<QString,quint32>("%1h",3600) <<
                                                    QPair<QString,quint32>("%1m",60) <<
                                                    QPair<QString,quint32>("%1s",1);
                                                    
                                                    
QString Utility::timeToDescriptiveString(quint64 msecs, quint8 precision, QString separator, bool specific) 
{     
    return timeToDescriptiveString( timeMapping , msecs, precision, separator, specific);
}


QString Utility::timeToDescriptiveString(QList<QPair<QString,quint32> > &timeMapping, quint64 msecs, quint8 precision, QString separator, bool specific)
{       
    quint64 secs = msecs / 1000;
    QString retStr = QString(timeMapping.last().first).arg(0); // default value in case theres no actual time in msecs.
    QList<QPair<QString,quint32> > values;
    bool timeStartFound = false;
   
    for(QList<QPair<QString,quint32> >::Iterator itr = timeMapping.begin(); itr != timeMapping.end() && precision > 0; itr++) {
        quint64 result = secs / itr->second;        
        if(!timeStartFound) {
            if(result == 0 ) {
                continue;
            }
            retStr = "";
            timeStartFound= true;        
        }        
        secs -= result * itr->second;
        values.append(QPair<QString,quint32>(itr->first,result));
        precision--;
    }
    
    for(QList<QPair<QString,quint32> >::Iterator itr = values.begin(); itr < values.end(); itr++) {
        retStr = retStr.append((specific || itr == values.end()-1) ? itr->first : "%1").arg(itr->second, (itr == values.begin() ? 1 :2 ), 10, QChar('0'));        
        if(itr < values.end()-1) {
            retStr.append(separator);
        }
        
            
    }
    
    return retStr;
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
#ifdef Q_OS_LINUX
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

void Utility::StopWatch::stop()
{
    addLapTime(QLatin1String(STOPWATCH_END_TAG));
    _timer.invalidate();
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
