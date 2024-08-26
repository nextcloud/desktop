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

#include "common/asserts.h"
#include "common/filesystembase.h"
#include "common/utility.h"
#include "common/version.h"

// Note:  This file must compile without QtGui
#include <QCollator>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>
#include <QTimeZone>
#include <QUrl>

#ifdef Q_OS_UNIX
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <cstring>
#include <math.h>
#include <stdarg.h>

using namespace std::chrono;
namespace {
auto RFC1123PatternC()
{
    return QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'");
}
}
namespace OCC {

Q_LOGGING_CATEGORY(lcUtility, "sync.utility", QtMsgType::QtInfoMsg)

QString Utility::octetsToString(qint64 octets)
{
    OC_ASSERT(octets >= 0)

    using namespace FileSystem::SizeLiterals;

    // We do what macOS 10.8 and above do: 0 fraction digits for bytes and KB; 1 fraction digits for MB; 2 for GB and above.
    // See also https://developer.apple.com/documentation/foundation/nsbytecountformatter/1417887-adaptive
    int precision = 0;
    if (quint64(octets) >= 1_GiB) {
        precision = 2;
    } else if (quint64(octets) >= 1_MiB) {
        precision = 1;
    }

    return QLocale().formattedDataSize(octets, precision, QLocale::DataSizeTraditionalFormat);
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
        .arg(platform(), OCC::Version::displayString(),
            // accessing the theme to fetch the string is rather difficult
            // since this is only needed server-side to identify clients, the app name (as of 2.9, the short name) is good enough
            qApp->applicationName(), QSysInfo::productType(), QSysInfo::kernelVersion(), QSysInfo::buildCpuArchitecture(), Utility::currentCpuArch())
        .toLatin1();
}

qint64 Utility::freeDiskSpace(const QString &path)
{
#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD) || defined(Q_OS_FREEBSD_KERNEL) || defined(Q_OS_NETBSD) || defined(Q_OS_OPENBSD)
    struct statvfs stat;
    if (statvfs(path.toLocal8Bit().data(), &stat) == 0) {
        return (qint64)stat.f_bavail * stat.f_frsize;
    }
#elif defined(Q_OS_UNIX)
    struct statvfs64 stat;
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
    const QString decPoint = locale.decimalPoint();
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

// This can be overriden from the tests
OCSYNC_EXPORT bool fsCasePreserving_override = []() -> bool {
    static bool ok = false;
    static int env = qEnvironmentVariableIntValue("OWNCLOUD_TEST_CASE_PRESERVING", &ok);
    if (ok) {
        return env;
    }
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
    // ONLY use this function with existing pathes.
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
    constexpr struct
    {
        const char *const name;
        const std::chrono::milliseconds msec;
        QString description(int value) const { return QCoreApplication::translate("Utility", name, nullptr, value); }
    } periods[] = {
        {QT_TRANSLATE_N_NOOP("Utility", "%n year(s)"), 24h * 365},
        {QT_TRANSLATE_N_NOOP("Utility", "%n month(s)"), 24h * 30},
        {QT_TRANSLATE_N_NOOP("Utility", "%n day(s)"), 24h},
        {QT_TRANSLATE_N_NOOP("Utility", "%n hour(s)"), 1h},
        {QT_TRANSLATE_N_NOOP("Utility", "%n minute(s)"), 1min},
        {QT_TRANSLATE_N_NOOP("Utility", "%n second(s)"), 1s},
        {nullptr, {}},
    };
} // anonymous namespace

QString Utility::durationToDescriptiveString2(std::chrono::milliseconds msecs)
{
    int p = 0;
    while (periods[p + 1].name && msecs < periods[p].msec) {
        p++;
    }

    const QString firstPart = periods[p].description(static_cast<int>(msecs / periods[p].msec));

    if (!periods[p + 1].name) {
        return firstPart;
    }

    const quint64 secondPartNum = qRound(static_cast<double>(msecs.count() % periods[p].msec.count()) / periods[p + 1].msec.count());

    if (secondPartNum == 0) {
        return firstPart;
    }

    return QCoreApplication::translate("Utility", "%1 %2").arg(firstPart, periods[p + 1].description(secondPartNum));
}

QString Utility::durationToDescriptiveString1(std::chrono::milliseconds msecs)
{
    int p = 0;
    while (periods[p + 1].name && msecs < periods[p].msec) {
        p++;
    }
    return periods[p].description(qRound(static_cast<double>(msecs.count()) / periods[p].msec.count()));
}

QString Utility::fileNameForGuiUse(const QString &fName)
{
    if (isMac()) {
        QString n(fName);
        return n.replace(QLatin1Char(':'), QLatin1Char('/'));
    }
    return fName;
}

QString Utility::normalizeEtag(QStringView etag)
{
    if (etag.isEmpty()) {
        return {};
    }

    const auto unQuote = [&] {
        if (etag.startsWith(QLatin1Char('"')) && etag.endsWith(QLatin1Char('"'))) {
            etag = etag.mid(1);
            etag.chop(1);
        }
    };

    // Weak E-Tags can appear when gzip compression is on, see #3946
    if (etag.startsWith(QLatin1String("W/"))) {
        etag = etag.mid(2);
    }
    /* strip normal quotes and quotes around "XXXX-gzip" */
    unQuote();

    // https://github.com/owncloud/client/issues/1195
    const QLatin1String gzipSuffix("-gzip");
    if (etag.endsWith(gzipSuffix)) {
        etag.chop(gzipSuffix.size());
    }
    /* strip normal quotes */
    unQuote();
    return etag.toString();
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

QString Utility::timeAgoInWords(const QDateTime &dt, const QDateTime &from)
{
    const QDateTime now = from.isValid() ? from : QDateTime::currentDateTimeUtc();

    if (dt.daysTo(now) > 0) {
        return QObject::tr("%n day(s) ago", "", dt.daysTo(now));
    }

    const qint64 secs = dt.secsTo(now);
    if (secs < 0) {
        return QObject::tr("in the future");
    }

    if (floor(secs / 3600.0) > 0) {
        const int hours = floor(secs / 3600.0);
        return (QObject::tr("%n hour(s) ago", "", hours));
    }

    const int minutes = qRound(secs / 60.0);
    if (minutes == 0) {
        if (secs < 5) {
            return QObject::tr("now");
        } else {
            return QObject::tr("less than a minute ago");
        }
    }

    return (QObject::tr("%n minute(s) ago", "", minutes));
}

void Utility::sortFilenames(QStringList &fileNames)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(fileNames.begin(), fileNames.end(), collator);
}

QString Utility::concatUrlPathItems(QStringList &&items, const QLatin1Char delimiter)
{
    for (QString &item : items) {
        while (item.endsWith(QLatin1Char('/'))) {
            item.chop(1);
        }
    }
    return items.join(delimiter);
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
    Q_ASSERT(!path.contains(QStringLiteral("//")));
    Q_ASSERT(url.query().isEmpty());
    QUrl tmpUrl = url;
    tmpUrl.setPath(path);
    tmpUrl.setQuery(queryItems);
    return tmpUrl;
}

bool Utility::urlEqual(QUrl url1, QUrl url2)
{
    // ensure https://demo.owncloud.org/ matches https://demo.owncloud.org
    // the empty path was the legacy formating before 2.9
    if (url1.path().isEmpty()) {
        url1.setPath(QStringLiteral("/"));
    }
    if (url2.path().isEmpty()) {
        url2.setPath(QStringLiteral("/"));
    }

    return url1.matches(url2, QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
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

bool Utility::isConflictFile(QStringView name)
{
    auto bname = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);

    if (bname.contains(QStringLiteral("_conflict-")))
        return true;

    if (bname.contains(QStringLiteral("(conflicted copy")))
        return true;

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

QString Utility::sanitizeForFileName(const QString &name)
{
    const auto invalid = QStringLiteral("/?<>\\:*|\"");
    QString result;
    result.reserve(name.size());
    for (const auto &c : name) {
        if (!invalid.contains(c) && c.category() != QChar::Other_Control && c.category() != QChar::Other_Format) {
            result.append(c);
        }
    }
    return result;
}

QString Utility::appImageLocation()
{
#ifdef Q_OS_LINUX
    Q_ASSERT(Utility::runningInAppImage());
    static const auto value = qEnvironmentVariable("APPIMAGE");
    return value;
#else
    Q_UNREACHABLE();
#endif
}

bool Utility::runningInAppImage()
{
#ifdef Q_OS_LINUX
    return qEnvironmentVariableIsSet("APPIMAGE");
#else
    return false;
#endif
}

QDateTime Utility::parseRFC1123Date(const QString &date)
{
    if (!date.isEmpty()) {
        auto out = QDateTime::fromString(date, RFC1123PatternC());
        Q_ASSERT(out.isValid());
        out.setTimeZone(QTimeZone::utc());
        return out;
    }
    return {};
}

QString Utility::formatRFC1123Date(const QDateTime &date)
{
    return date.toUTC().toString(RFC1123PatternC());
}

#ifndef Q_OS_MAC
QString Utility::currentCpuArch()
{
    return QSysInfo::currentCpuArchitecture();
}
#endif

} // namespace OCC

QDebug operator<<(QDebug debug, nanoseconds in)
{
    QDebugStateSaver save(debug);
    debug.nospace();
    const auto h = duration_cast<hours>(in);
    const auto min = duration_cast<minutes>(in -= h);
    const auto s = duration_cast<seconds>(in -= min);
    const auto ms = duration_cast<milliseconds>(in -= s);
    return debug << "duration("
                 << h.count() << "h, "
                 << min.count() << "min, "
                 << s.count() << "s, "
                 << ms.count() << "ms)";
}
