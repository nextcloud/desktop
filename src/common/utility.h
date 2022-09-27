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

#ifndef UTILITY_H
#define UTILITY_H

#include "ocsynclib.h"

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMap>
#include <QMetaEnum>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include <functional>
#include <memory>
#include <optional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class QSettings;

namespace OCC {

class SyncJournal;

OCSYNC_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcUtility)

    /** \addtogroup libsync
 *  @{
 */
    namespace Utility
{
    OCSYNC_EXPORT void sleep(int sec);
    OCSYNC_EXPORT void usleep(int usec);
    OCSYNC_EXPORT QString formatFingerprint(const QByteArray &, bool colonSeparated = true);
    OCSYNC_EXPORT void setupFavLink(const QString &folder);
    OCSYNC_EXPORT QString octetsToString(qint64 octets);
    OCSYNC_EXPORT QByteArray userAgentString();
    /**
      * @brief Return whether launch on startup is enabled system wide.
      *
      * If this returns true, the checkbox for user specific launch
      * on startup will be hidden.
      *
      * Currently only implemented on Windows.
      */
    OCSYNC_EXPORT bool hasSystemLaunchOnStartup(const QString &appName);
    OCSYNC_EXPORT bool hasLaunchOnStartup(const QString &appName);
    OCSYNC_EXPORT void setLaunchOnStartup(const QString &appName, const QString &guiName, bool launch);

    /**
     * Return the amount of free space available.
     *
     * \a path must point to a directory
     */
    OCSYNC_EXPORT qint64 freeDiskSpace(const QString &path);

    /**
     * @brief compactFormatDouble - formats a double value human readable.
     *
     * @param value the value to format.
     * @param prec the precision.
     * @param unit an optional unit that is appended if present.
     * @return the formatted string.
     */
    OCSYNC_EXPORT QString compactFormatDouble(double value, int prec, const QString &unit = QString());

    // porting methods
    OCSYNC_EXPORT QString escape(const QString &);

    // conversion function QDateTime <-> time_t   (because the ones builtin work on only unsigned 32bit)
    OCSYNC_EXPORT QDateTime qDateTimeFromTime_t(qint64 t);
    OCSYNC_EXPORT qint64 qDateTimeToTime_t(const QDateTime &t);

    /**
     * @brief Convert milliseconds duration to human readable string.
     * @param quint64 msecs the milliseconds to convert to string.
     * @return an HMS representation of the milliseconds value.
     *
     * durationToDescriptiveString1 describes the duration in a single
     * unit, like "5 minutes" or "2 days".
     *
     * durationToDescriptiveString2 uses two units where possible, so
     * "5 minutes 43 seconds" or "1 month 3 days".
     */
    OCSYNC_EXPORT QString durationToDescriptiveString1(quint64 msecs);
    OCSYNC_EXPORT QString durationToDescriptiveString2(quint64 msecs);

    /**
     * @brief hasDarkSystray - determines whether the systray is dark or light.
     *
     * Use this to check if the OS has a dark or a light systray.
     *
     * The value might change during the execution of the program
     * (e.g. on OS X 10.10).
     *
     * @return bool which is true for systems with dark systray.
     */
    OCSYNC_EXPORT bool hasDarkSystray();

    // convenience OS detection methods
    inline bool isWindows();
    inline bool isMac();
    inline bool isUnix();
    inline bool isLinux(); // use with care
    inline bool isBSD(); // use with care, does not match OS X

    OCSYNC_EXPORT QString platformName();
    // crash helper for --debug
    OCSYNC_EXPORT void crash();

    // Case preserving file system underneath?
    // if this function returns true, the file system is case preserving,
    // that means "test" means the same as "TEST" for filenames.
    // if false, the two cases are two different files.
    OCSYNC_EXPORT bool fsCasePreserving();
    inline auto fsCaseSensitivity()
    {
        return fsCasePreserving() ? Qt::CaseInsensitive : Qt::CaseSensitive;
    }

    // Check if two pathes that MUST exist are equal. This function
    // uses QDir::canonicalPath() to judge and cares for the systems
    // case sensitivity.
    OCSYNC_EXPORT bool fileNamesEqual(const QString &fn1, const QString &fn2);

    inline auto stripTrailingSlash(QStringView s)
    {
        if (s.endsWith(QLatin1Char('/'))) {
            s.chop(1);
        }
        return s.toString();
    }

    // Call the given command with the switch --version and rerun the first line
    // of the output.
    // If command is empty, the function calls the running application which, on
    // Linux, might have changed while this one is running.
    // For Mac and Windows, it returns QString()
    OCSYNC_EXPORT QByteArray versionOfInstalledBinary(const QString &command = QString());

    OCSYNC_EXPORT QString fileNameForGuiUse(const QString &fName);

    OCSYNC_EXPORT QByteArray normalizeEtag(QByteArray etag);

    /**
     * @brief timeAgoInWords - human readable time span
     *
     * Use this to get a string that describes the timespan between the first and
     * the second timestamp in a human readable and understandable form.
     *
     * If the second parameter is ommitted, the current time is used.
     */
    OCSYNC_EXPORT QString timeAgoInWords(const QDateTime &dt, const QDateTime &from = QDateTime());

    class OCSYNC_EXPORT StopWatch
    {
    private:
        QMap<QString, quint64> _lapTimes;
        QDateTime _startTime;
        QElapsedTimer _timer;

    public:
        void start();
        quint64 stop();
        quint64 addLapTime(const QString &lapName);
        void reset();

        // out helpers, return the measured times.
        QDateTime startTime() const;
        QDateTime timeOfLap(const QString &lapName) const;
        quint64 durationOfLap(const QString &lapName) const;
    };

    /**
     * @brief Sort a QStringList in a way that's appropriate for filenames
     */
    OCSYNC_EXPORT void sortFilenames(QStringList &fileNames);

    /** Appends concatPath and queryItems to the url */
    OCSYNC_EXPORT QUrl concatUrlPath(
        const QUrl &url, const QString &concatPath,
        const QUrlQuery &queryItems = {});

    /** Compares two urls and ignores whether thei end wit / */
    OCSYNC_EXPORT bool urlEqual(QUrl a, QUrl b);

    /**  Returns a new settings pre-set in a specific group.  The Settings will be created
         with the given parent. If no parent is specified, the caller must destroy the settings */
    OCSYNC_EXPORT std::unique_ptr<QSettings> settingsWithGroup(const QString &group, QObject *parent = nullptr);

    /** Sanitizes a string that shall become part of a filename.
     *
     * Filters out reserved characters like
     * - unicode control and format characters
     * - reserved characters: /, ?, <, >, \, :, *, |, and "
     *
     * Warning: This does not sanitize the whole resulting string, so
     * - unix reserved filenames ('.', '..')
     * - trailing periods and spaces
     * - windows reserved filenames ('CON' etc)
     * will pass unchanged.
     */
    OCSYNC_EXPORT QString sanitizeForFileName(const QString &name);

    /** Returns a file name based on \a fn that's suitable for a conflict.
     */
    OCSYNC_EXPORT QString makeConflictFileName(
        const QString &fn, const QDateTime &dt, const QString &user);

    /** Returns whether a file name indicates a conflict file
     */
    OCSYNC_EXPORT bool isConflictFile(const QStringRef &name);
    inline auto isConflictFile(const QString &name)
    {
        return isConflictFile(&name);
    }

    /** Find the base name for a conflict file name, using name pattern only
     *
     * Will return an empty string if it's not a conflict file.
     *
     * Prefer to use the data from the conflicts table in the journal to determine
     * a conflict's base file, see SyncJournal::conflictFileBaseName()
     */
    OCSYNC_EXPORT QByteArray conflictFileBaseNameFromPattern(const QByteArray &conflictName);

#ifdef Q_OS_WIN
    OCSYNC_EXPORT QVariant registryGetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName);
    OCSYNC_EXPORT bool registrySetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName, DWORD type, const QVariant &value);
    OCSYNC_EXPORT bool registryDeleteKeyTree(HKEY hRootKey, const QString &subKey);
    OCSYNC_EXPORT bool registryDeleteKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName);
    OCSYNC_EXPORT bool registryWalkSubKeys(HKEY hRootKey, const QString &subKey, const std::function<void(HKEY, const QString &)> &callback);

    // Possibly refactor to share code with UnixTimevalToFileTime in c_time.c
    OCSYNC_EXPORT void UnixTimeToFiletime(time_t t, FILETIME *filetime);
    OCSYNC_EXPORT void FiletimeToLargeIntegerFiletime(FILETIME *filetime, LARGE_INTEGER *hundredNSecs);
    OCSYNC_EXPORT void UnixTimeToLargeIntegerFiletime(time_t t, LARGE_INTEGER *hundredNSecs);

    OCSYNC_EXPORT QString formatWinError(long error);

    class OCSYNC_EXPORT NtfsPermissionLookupRAII
    {
    public:
        /**
         * NTFS permissions lookup is diabled by default for performance reasons
         * Enable it and disable it again once we leave the scope
         * https://doc.qt.io/Qt-5/qfileinfo.html#ntfs-permissions
         */
        NtfsPermissionLookupRAII();
        ~NtfsPermissionLookupRAII();

    private:
        Q_DISABLE_COPY(NtfsPermissionLookupRAII);
    };


    class OCSYNC_EXPORT Handle
    {
    public:
        /**
         * A RAAI for Windows Handles
         */
        Handle() = default;
        explicit Handle(HANDLE h);
        explicit Handle(HANDLE h, std::function<void(HANDLE)> &&close);

        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;

        Handle(Handle &&other)
        {
            std::swap(_handle, other._handle);
            std::swap(_close, other._close);
        }

        Handle &operator=(Handle &&other)
        {
            if (this != &other) {
                std::swap(_handle, other._handle);
                std::swap(_close, other._close);
            }
            return *this;
        }

        ~Handle();

        HANDLE &handle()
        {
            return _handle;
        }

        void close();

        explicit operator bool() const
        {
            return _handle != INVALID_HANDLE_VALUE;
        }

        operator HANDLE() const
        {
            return _handle;
        }

    private:
        HANDLE _handle = INVALID_HANDLE_VALUE;
        std::function<void(HANDLE)> _close;
    };

#endif

    template <class E>
    E stringToEnum(const char *key)
    {
        return static_cast<E>(QMetaEnum::fromType<E>().keyToValue(key));
    }

    template <class E>
    E stringToEnum(const QString &key)
    {
        return stringToEnum<E>(key.toUtf8().constData());
    }

    template <class E>
    QString enumToString(E value)
    {
        return QString::fromUtf8(QMetaEnum::fromType<E>().valueToKeys(value));
    }

    template <class E = void>
    QString enumToDisplayName(E)
    {
        static_assert(std::is_same<E, void>::value, "Not implemented");
        Q_UNREACHABLE();
    }

    /**
     * Replace all occurances of @{} values in template with the values from values
     */
    OCSYNC_EXPORT QString renderTemplate(QString templ, const QMap<QString, QString> &values);

    /**
     * Perform a const find on a Qt container and returns an std::optional<const_iterator>
     * This allows performant access to the container with in a simple if condition.
     * if (auto it = optionalFind("key"))
     */
    template <typename T>
    auto optionalFind(const T &container, const QString &key)
    {
        auto it = container.constFind(key);
        return it == container.cend() ? std::nullopt : std::make_optional(it);
    }


#ifdef Q_OS_LINUX
    OCSYNC_EXPORT QString appImageLocation();
    OCSYNC_EXPORT bool runningInAppImage();
#endif
}
/** @} */ // \addtogroup

inline bool Utility::isWindows()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

inline bool Utility::isMac()
{
#ifdef Q_OS_MAC
    return true;
#else
    return false;
#endif
}

inline bool Utility::isUnix()
{
#ifdef Q_OS_UNIX
    return true;
#else
    return false;
#endif
}

inline bool Utility::isLinux()
{
#if defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

inline bool Utility::isBSD()
{
#if defined(Q_OS_FREEBSD) || defined(Q_OS_NETBSD) || defined(Q_OS_OPENBSD)
    return true;
#else
    return false;
#endif
}
}
#endif // UTILITY_H

OCSYNC_EXPORT QDebug &operator<<(QDebug &debug, std::chrono::nanoseconds in);