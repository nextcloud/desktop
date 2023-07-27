/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "config.h"

#include "common/utility.h"
#include "common/filesystembase.h"
#include "version.h"

// Note:  This file must compile without QtGui
#include <QCoreApplication>
#include <QSettings>
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QProcess>
#include <QObject>
#include <QThread>
#include <QDateTime>
#include <QSysInfo>
#include <QStandardPaths>
#include <QCollator>
#include <QSysInfo>
#include <qrandom.h>

#ifdef Q_OS_UNIX
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <cmath>
#include <cstdarg>
#include <cstring>

namespace {
constexpr auto bytes = 1024;
constexpr auto kilobytes = bytes;
constexpr auto megabytes = bytes * kilobytes;
constexpr qint64 gigabytes = bytes * megabytes;
constexpr qint64 terabytes = bytes * gigabytes;
}

namespace OCC {

Q_LOGGING_CATEGORY(lcUtility, "nextcloud.sync.utility", QtInfoMsg)

bool Utility::writeRandomFile(const QString &fname, int size)
{
    int maxSize = 10 * 10 * 1024;

    if (size == -1)
        size = rand() % maxSize;

    QString randString;
    for (int i = 0; i < size; i++) {
        int r = rand() % 128;
        randString.append(QChar(r));
    }

    QFile file(fname);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << randString;
        // optional, as QFile destructor will already do it:
        file.close();
        return true;
    }
    return false;
}

QString Utility::formatFingerprint(const QByteArray &fmhash, bool colonSeparated)
{
    QByteArray hash;
    int steps = fmhash.length() / 2;
    for (int i = 0; i < steps; i++) {
        hash.append(fmhash[i * 2]);
        hash.append(fmhash[i * 2 + 1]);
        hash.append(' ');
    }

    QString fp = QString::fromLatin1(hash.trimmed());
    if (colonSeparated) {
        fp.replace(QLatin1Char(' '), QLatin1Char(':'));
    }

    return fp;
}


QString Utility::octetsToString(const qint64 octets)
{
    auto unitName = QCoreApplication::translate("Utility", "%L1 B");
    qreal dataSize = octets;

    // Display decimals when value < TB and unit < 10
    auto showDecimals = true;

    if (octets >= terabytes) {
        unitName = QCoreApplication::translate("Utility", "%L1 TB");
        dataSize /= terabytes;
        showDecimals = false;
    } else if (octets >= gigabytes) {
        unitName = QCoreApplication::translate("Utility", "%L1 GB");
        dataSize /= gigabytes;
        showDecimals = false;
    } else if (octets >= megabytes) {
        unitName = QCoreApplication::translate("Utility", "%L1 MB");
        dataSize /= megabytes;
        showDecimals = false;
    } else if (octets >= kilobytes) {
        unitName = QCoreApplication::translate("Utility", "%L1 KB");
        dataSize /= kilobytes;
    }

    if (dataSize > 9.95) {
        showDecimals = true;
    }

    if (showDecimals) {
        return unitName.arg(qRound(dataSize));
    }

    return unitName.arg(dataSize, 0, 'g', 2);
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
    return QSysInfo::productType();
#endif
}

QByteArray Utility::userAgentString()
{
    return QStringLiteral("Mozilla/5.0 (%1) mirall/%2 (%3, %4-%5 ClientArchitecture: %6 OsArchitecture: %7)")
        .arg(platform(),
            QStringLiteral(MIRALL_VERSION_STRING),
            qApp->applicationName(),
            QSysInfo::productType(),
            QSysInfo::kernelVersion(),
            QSysInfo::buildCpuArchitecture(),
            QSysInfo::currentCpuArchitecture())
        .toLatin1();
}

QByteArray Utility::friendlyUserAgentString()
{
    const auto pattern = QStringLiteral("%1 (Desktop Client - %2)");
    const auto userAgent = pattern.arg(QSysInfo::machineHostName(), platform());
    return userAgent.toUtf8();
}

qint64 Utility::freeDiskSpace(const QString &path)
{
#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD) || defined(Q_OS_FREEBSD_KERNEL) || defined(Q_OS_NETBSD) || defined(Q_OS_OPENBSD)
    struct statvfs stat;
    if (statvfs(path.toLocal8Bit().data(), &stat) == 0) {
        return (qint64)stat.f_bavail * stat.f_frsize;
    }
#elif defined(Q_OS_UNIX)
    struct statvfs64 stat{};
    if (statvfs64(path.toLocal8Bit().data(), &stat) == 0) {
        return (qint64)stat.f_bavail * stat.f_frsize;
    }
#elif defined(Q_OS_WIN)
    ULARGE_INTEGER freeBytes;
    freeBytes.QuadPart = 0L;
    if (GetDiskFreeSpaceEx(reinterpret_cast<const wchar_t *>(FileSystem::longWinPath(path).utf16()), &freeBytes, nullptr, nullptr)) {
        return freeBytes.QuadPart;
    }
#endif
    return -1;
}

QString Utility::compactFormatDouble(double value, int prec, const QString &unit)
{
    QLocale locale = QLocale::system();
    const auto decPoint = locale.decimalPoint();
    QString str = locale.toString(value, 'f', prec);
    while (str.endsWith(QLatin1Char('0')) || str.endsWith(decPoint)) {
        if (str.endsWith(decPoint)) {
            str.chop(1);
            break;
        }
        str.chop(1);
    }
    if (!unit.isEmpty())
        str += (QLatin1Char(' ') + unit);
    return str;
}

QString Utility::escape(const QString &in)
{
    return in.toHtmlEscaped();
}

int Utility::rand()
{
    return QRandomGenerator::global()->bounded(0, RAND_MAX);
}

void Utility::sleep(int sec)
{
    QThread::sleep(sec);
}

void Utility::usleep(int usec)
{
    QThread::usleep(usec);
}

// This can be overridden from the tests
OCSYNC_EXPORT bool fsCasePreserving_override = []() -> bool {
    QByteArray env = qgetenv("OWNCLOUD_TEST_CASE_PRESERVING");
    if (!env.isEmpty())
        return env.toInt();
    return Utility::isWindows() || Utility::isMac();
}();

bool Utility::fsCasePreserving()
{
    return fsCasePreserving_override;
}

bool Utility::fileNamesEqual(const QString &fn1, const QString &fn2)
{
    const QDir fd1(fn1);
    const QDir fd2(fn2);

    // Attention: If the path does not exist, canonicalPath returns ""
    // ONLY use this function with existing paths.
    const QString a = fd1.canonicalPath();
    const QString b = fd2.canonicalPath();
    bool re = !a.isEmpty() && QString::compare(a, b, fsCasePreserving() ? Qt::CaseInsensitive : Qt::CaseSensitive) == 0;
    return re;
}

QDateTime Utility::qDateTimeFromTime_t(qint64 t)
{
    return QDateTime::fromMSecsSinceEpoch(t * 1000);
}

qint64 Utility::qDateTimeToTime_t(const QDateTime &t)
{
    return t.toMSecsSinceEpoch() / 1000;
}

namespace {
    struct Period
    {
        const char *name;
        quint64 msec;

        [[nodiscard]] QString description(quint64 value) const
        {
            return QCoreApplication::translate("Utility", name, nullptr, value);
        }
    };
// QTBUG-3945 and issue #4855: QT_TRANSLATE_NOOP does not work with plural form because lupdate
// limitation unless we fake more arguments
// (it must be in the form ("context", "source", "comment", n)
#undef QT_TRANSLATE_NOOP
#define QT_TRANSLATE_NOOP(ctx, str, ...) str
    Q_DECL_CONSTEXPR Period periods[] = {
        { QT_TRANSLATE_NOOP("Utility", "%n year(s)", 0, _), 365 * 24 * 3600 * 1000LL },
        { QT_TRANSLATE_NOOP("Utility", "%n month(s)", 0, _), 30 * 24 * 3600 * 1000LL },
        { QT_TRANSLATE_NOOP("Utility", "%n day(s)", 0, _), 24 * 3600 * 1000LL },
        { QT_TRANSLATE_NOOP("Utility", "%n hour(s)", 0, _), 3600 * 1000LL },
        { QT_TRANSLATE_NOOP("Utility", "%n minute(s)", 0, _), 60 * 1000LL },
        { QT_TRANSLATE_NOOP("Utility", "%n second(s)", 0, _), 1000LL },
        { nullptr, 0 }
    };
} // anonymous namespace

QString Utility::durationToDescriptiveString2(quint64 msecs)
{
    int p = 0;
    while (periods[p + 1].name && msecs < periods[p].msec) {
        p++;
    }

    auto firstPart = periods[p].description(int(msecs / periods[p].msec));

    if (!periods[p + 1].name) {
        return firstPart;
    }

    quint64 secondPartNum = qRound(double(msecs % periods[p].msec) / periods[p + 1].msec);

    if (secondPartNum == 0) {
        return firstPart;
    }

    return QCoreApplication::translate("Utility", "%1 %2").arg(firstPart, periods[p + 1].description(secondPartNum));
}

QString Utility::durationToDescriptiveString1(quint64 msecs)
{
    int p = 0;
    while (periods[p + 1].name && msecs < periods[p].msec) {
        p++;
    }

    quint64 amount = qRound(double(msecs) / periods[p].msec);
    return periods[p].description(amount);
}

QString Utility::fileNameForGuiUse(const QString &fName)
{
    if (isMac()) {
        QString n(fName);
        return n.replace(QLatin1Char(':'), QLatin1Char('/'));
    }
    return fName;
}

QByteArray Utility::normalizeEtag(QByteArray etag)
{
    /* strip "XXXX-gzip" */
    if (etag.startsWith('"') && etag.endsWith("-gzip\"")) {
        etag.chop(6);
        etag.remove(0, 1);
    }
    /* strip trailing -gzip */
    if (etag.endsWith("-gzip")) {
        etag.chop(5);
    }
    /* strip normal quotes */
    if (etag.startsWith('"') && etag.endsWith('"')) {
        etag.chop(1);
        etag.remove(0, 1);
    }
    etag.squeeze();
    return etag;
}

QString Utility::platformName()
{
    return QSysInfo::prettyProductName();
}

void Utility::crash()
{
    volatile int *a = (int *)nullptr;
    *a = 1;
}

// Use this functions to retrieve uint/int (often required by Qt and WIN32) from size_t
// without compiler warnings about possible truncation
uint Utility::convertSizeToUint(size_t &convertVar)
{
    if (convertVar > UINT_MAX) {
        //throw std::bad_cast();
        convertVar = UINT_MAX; // intentionally default to wrong value here to not crash: exception handling TBD
    }
    return static_cast<uint>(convertVar);
}

int Utility::convertSizeToInt(size_t &convertVar)
{
    if (convertVar > INT_MAX) {
        //throw std::bad_cast();
        convertVar = INT_MAX; // intentionally default to wrong value here to not crash: exception handling TBD
    }
    return static_cast<int>(convertVar);
}

// read the output of the owncloud --version command from the owncloud
// version that is on disk. This works for most versions of the client,
// because clients that do not yet know the --version flag return the
// version in the first line of the help output :-)
//
// This version only delivers output on linux, as Mac and Win get their
// restarting from the installer.
QByteArray Utility::versionOfInstalledBinary(const QString &command)
{
    QByteArray re;
    if (isLinux()) {
        QString binary(command);
        if (binary.isEmpty()) {
            binary = qApp->arguments()[0];
        }
        QStringList params;
        params << QStringLiteral("--version");
        QProcess process;
        process.start(binary, params);
        process.waitForFinished(); // sets current thread to sleep and waits for pingProcess end
        re = process.readAllStandardOutput();
        int newline = re.indexOf('\n');
        if (newline > 0) {
            re.truncate(newline);
        }
    }
    return re;
}

QString Utility::timeAgoInWords(const QDateTime &dt, const QDateTime &from)
{
    QDateTime now = QDateTime::currentDateTimeUtc();

    if (from.isValid()) {
        now = from;
    }

    if (dt.daysTo(now) >= 1) {
        return QObject::tr("%nd", "delay in days after an activity", dt.daysTo(now));
    } else {
        qint64 secs = dt.secsTo(now);
        if (secs < 0) {
            return QObject::tr("in the future");
        }

        if (floor(secs / 3600.0) > 0) {
            int hours = floor(secs / 3600.0);
            return (QObject::tr("%nh", "delay in hours after an activity", hours));
        } else {
            int minutes = qRound(secs / 60.0);

            if (minutes == 0) {
                if (secs < 5) {
                    return QObject::tr("now");
                } else {
                    return QObject::tr("1m", "one minute after activity date and time");
                }
            } else {
                return (QObject::tr("%nm", "delay in minutes after an activity", minutes));
            }
        }
    }
    return QObject::tr("Some time ago");
}

/* --------------------------------------------------------------------------- */

static const char STOPWATCH_END_TAG[] = "_STOPWATCH_END";

void Utility::StopWatch::start()
{
    _startTime = QDateTime::currentDateTimeUtc();
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

quint64 Utility::StopWatch::addLapTime(const QString &lapName)
{
    if (!_timer.isValid()) {
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

QDateTime Utility::StopWatch::timeOfLap(const QString &lapName) const
{
    quint64 t = durationOfLap(lapName);
    if (t) {
        QDateTime re(_startTime);
        return re.addMSecs(t);
    }

    return QDateTime();
}

quint64 Utility::StopWatch::durationOfLap(const QString &lapName) const
{
    return _lapTimes.value(lapName, 0);
}

void Utility::sortFilenames(QStringList &fileNames)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(fileNames.begin(), fileNames.end(), collator);
}

QUrl Utility::concatUrlPath(const QUrl &url, const QString &concatPath,
    const QUrlQuery &queryItems)
{
    QString path = url.path();
    if (!concatPath.isEmpty()) {
        // avoid '//'
        if (path.endsWith(QLatin1Char('/')) && concatPath.startsWith(QLatin1Char('/'))) {
            path.chop(1);
        } // avoid missing '/'
        else if (!path.endsWith(QLatin1Char('/')) && !concatPath.startsWith(QLatin1Char('/'))) {
            path += QLatin1Char('/');
        }
        path += concatPath; // put the complete path together
    }

    QUrl tmpUrl = url;
    tmpUrl.setPath(path);
    tmpUrl.setQuery(queryItems);
    return tmpUrl;
}

QString Utility::makeConflictFileName(
    const QString &fn, const QDateTime &dt, const QString &user)
{
    QString conflictFileName(fn);
    // Add conflict tag before the extension.
    int dotLocation = conflictFileName.lastIndexOf(QLatin1Char('.'));
    // If no extension, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
    if (dotLocation <= conflictFileName.lastIndexOf(QLatin1Char('/')) + 1) {
        dotLocation = conflictFileName.size();
    }

    QString conflictMarker = QStringLiteral(" (conflicted copy ");
    if (!user.isEmpty()) {
        // Don't allow parens in the user name, to ensure
        // we can find the beginning and end of the conflict tag.
        const auto userName = sanitizeForFileName(user).replace(QLatin1Char('('), QLatin1Char('_')).replace(QLatin1Char(')'), QLatin1Char('_'));;
        conflictMarker += userName + QLatin1Char(' ');
    }
    conflictMarker += dt.toString(QStringLiteral("yyyy-MM-dd hhmmss")) + QLatin1Char(')');

    conflictFileName.insert(dotLocation, conflictMarker);
    return conflictFileName;
}

bool Utility::isConflictFile(const QString &name)
{
    auto bname = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);

    if (bname.contains(QStringLiteral("_conflict-"))) {
        return true;
    }

    if (bname.contains(QStringLiteral("(conflicted copy"))) {
        return true;
    }

    if (isCaseClashConflictFile(name)) {
        return true;
    }

    return false;
}

QByteArray Utility::conflictFileBaseNameFromPattern(const QByteArray &conflictName)
{
    // This function must be able to deal with conflict files for conflict files.
    // To do this, we scan backwards, for the outermost conflict marker and
    // strip only that to generate the conflict file base name.
    auto startOld = conflictName.lastIndexOf("_conflict-");

    // A single space before "(conflicted copy" is considered part of the tag
    auto startNew = conflictName.lastIndexOf("(conflicted copy");
    if (startNew > 0 && conflictName[startNew - 1] == ' ')
        startNew -= 1;

    // The rightmost tag is relevant
    auto tagStart = qMax(startOld, startNew);
    if (tagStart == -1)
        return "";

    // Find the end of the tag
    auto tagEnd = conflictName.size();
    auto dot = conflictName.lastIndexOf('.'); // dot could be part of user name for new tag!
    if (dot > tagStart)
        tagEnd = dot;
    if (tagStart == startNew) {
        auto paren = conflictName.indexOf(')', tagStart);
        if (paren != -1)
            tagEnd = paren + 1;
    }
    return conflictName.left(tagStart) + conflictName.mid(tagEnd);
}

bool Utility::isPathWindowsDrivePartitionRoot(const QString &path)
{
    Q_UNUSED(path)
#ifdef Q_OS_WIN
    // should be 2 or 3 characters length
    if (!(path.size() >= 2 && path.size() <= 3)) {
        return false;
    }

    // must match a pattern "[A-Za-z]:"
    if (!(path.at(1) == QLatin1Char(':') && path.at(0).isLetter())) {
        return false;
    }

    // final check - last character should be either slash/backslash, or, it should be missing
    return path.size() < 3 || path.at(2) == QLatin1Char('/') || path.at(2) == QLatin1Char('\\');
#endif
    return false;
}

QString Utility::sanitizeForFileName(const QString &name)
{
    const auto invalid = QStringLiteral(R"(/?<>\:*|")");
    QString result;
    result.reserve(name.size());
    for (const auto c : name) {
        if (!invalid.contains(c)
            && c.category() != QChar::Other_Control
            && c.category() != QChar::Other_Format) {
            result.append(c);
        }
    }
    return result;
}

QString Utility::makeCaseClashConflictFileName(const QString &filename, const QDateTime &datetime)
{
    auto conflictFileName(filename);
    // Add conflict tag before the extension.
    auto dotLocation = conflictFileName.lastIndexOf(QLatin1Char('.'));
    // If no extension, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
    if (dotLocation <= conflictFileName.lastIndexOf(QLatin1Char('/')) + 1) {
        dotLocation = conflictFileName.size();
    }

    auto conflictMarker = QStringLiteral(" (case clash from ");
    conflictMarker += datetime.toString(QStringLiteral("yyyy-MM-dd hhmmss")) + QLatin1Char(')');

    conflictFileName.insert(dotLocation, conflictMarker);
    return conflictFileName;
}

bool Utility::isCaseClashConflictFile(const QString &name)
{
    const auto bname = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);

    return bname.contains(QStringLiteral("(case clash from"));
}

QString Utility::leadingSlashPath(const QString &path)
{
    static const auto slash = QLatin1Char('/');
    return !path.startsWith(slash) ? QString(slash + path) : path;
}

QString Utility::trailingSlashPath(const QString &path)
{
    static const auto slash = QLatin1Char('/');
    return path.endsWith(slash) ? path : QString(path + slash);
}

QString Utility::noLeadingSlashPath(const QString &path)
{
    static const auto slash = QLatin1Char('/');
    return path.size() > 1 && path.startsWith(slash) ? path.mid(1) : path;
}

QString Utility::noTrailingSlashPath(const QString &path)
{
    static const auto slash = QLatin1Char('/');
    return path.size() > 1 && path.endsWith(slash) ? path.chopped(1) : path;
}

QString Utility::fullRemotePathToRemoteSyncRootRelative(const QString &fullRemotePath, const QString &remoteSyncRoot)
{
    const auto remoteSyncRootNoLeadingSlashWithTrailingSlash = Utility::trailingSlashPath(noLeadingSlashPath(remoteSyncRoot));
    const auto fullRemotePathNoLeadingSlash = noLeadingSlashPath(fullRemotePath);

    if (remoteSyncRootNoLeadingSlashWithTrailingSlash == QStringLiteral("/")) {
        return noLeadingSlashPath(noTrailingSlashPath(fullRemotePath));
    }

    if (!fullRemotePathNoLeadingSlash.startsWith(remoteSyncRootNoLeadingSlashWithTrailingSlash)) {
        return fullRemotePath;
    }

    const auto relativePathToRemoteSyncRoot = fullRemotePathNoLeadingSlash.mid(remoteSyncRootNoLeadingSlashWithTrailingSlash.size());
    Q_ASSERT(!relativePathToRemoteSyncRoot.isEmpty());
    return noLeadingSlashPath(noTrailingSlashPath(relativePathToRemoteSyncRoot));
}


} // namespace OCC
