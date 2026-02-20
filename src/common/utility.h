/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef UTILITY_H
#define UTILITY_H


#include "csync/ocsynclib.h"
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMap>
#include <QUrl>
#include <QUrlQuery>
#include <functional>
#include <memory>

#ifdef Q_OS_WIN
#include <QRect>
#include <windows.h>
#endif

class QSettings;

namespace OCC {

class SyncJournal;

Q_DECLARE_LOGGING_CATEGORY(lcUtility)

/** \addtogroup libsync
 *  @{
 */
namespace Utility {
    struct ProcessInfosForOpenFile {
        ulong processId;
        QString processName;
    };
    /**
     * @brief Queries the OS for processes that are keeping the file open(using it)
     *
     * @param filePath absolute file path
     * @return list of ProcessInfosForOpenFile
     */
    OCSYNC_EXPORT QVector<ProcessInfosForOpenFile> queryProcessInfosKeepingFileOpen(const QString &filePath);

    OCSYNC_EXPORT int rand();
    OCSYNC_EXPORT void sleep(int sec);
    OCSYNC_EXPORT void usleep(int usec);
    OCSYNC_EXPORT QString formatFingerprint(const QByteArray &, bool colonSeparated = true);
    /**
     * @brief Create favorite link for sync folder with application name and icon
     *
     * @param folder absolute file path to folder
     */
    OCSYNC_EXPORT void setupFavLink(const QString &folder);
    /**
     * @brief Migrate favorite link for sync folder with new application name and icon
     *
     * @param folder absolute file path to folder
     */
    OCSYNC_EXPORT void migrateFavLink(const QString &folder);
    /**
     * @brief Creates or overwrite the Desktop.ini file to use new folder IconResource shown as a favorite link
     *
     * @param folder absolute file path to folder
     * @param localizedResourceName new folder name to be used as display name (migration)
     */
    OCSYNC_EXPORT void setupDesktopIni(const QString &folder, const QString localizedResourceName = {});
    /**
     * @brief Removes the Desktop.ini file which contains the folder IconResource shown as a favorite link
     *
     * @param folder absolute file path to folder
     */
    OCSYNC_EXPORT void removeFavLink(const QString &folder);
    /**
     * @brief Return the display name of a folder - to be used in fav links and sync root name (VFS).x
     * e.g. Nextcloud1 will become NewAppName1, NewAppName2 or FolderName will be kept as is.
     *
     * @param currentDisplayName current folder display name string
     * @param newName new name to be used for the folder
     */
    OCSYNC_EXPORT QString syncFolderDisplayName(const QString &currentDisplayName, const QString &newName);

    // convenience system path to links folder
    OCSYNC_EXPORT QString systemPathToLinks();

    OCSYNC_EXPORT bool writeRandomFile(const QString &fname, int size = -1);
    OCSYNC_EXPORT QString octetsToString(const qint64 octets);
    OCSYNC_EXPORT QByteArray userAgentString();
    OCSYNC_EXPORT QByteArray friendlyUserAgentString();
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
    OCSYNC_EXPORT void setLaunchOnStartup(const QString &appName, const QString &guiName, const bool launch);
    OCSYNC_EXPORT uint convertSizeToUint(size_t &convertVar);
    OCSYNC_EXPORT int convertSizeToInt(size_t &convertVar);

#ifdef Q_OS_WIN
    OCSYNC_EXPORT DWORD convertSizeToDWORD(size_t &convertVar);
#endif

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
    constexpr bool isWindows();
    constexpr bool isMac();
    constexpr bool isUnix();
    constexpr bool isLinux(); // use with care
    constexpr bool isBSD(); // use with care, does not match OS X

    OCSYNC_EXPORT QString platformName();

    // AppImage helpers (Linux only, return empty/false elsewhere)
    OCSYNC_EXPORT QString appImagePath();
    OCSYNC_EXPORT bool isRunningInAppImage();
    // crash helper for --debug
    OCSYNC_EXPORT void crash();

    // Case preserving file system underneath?
    // if this function returns true, the file system is case preserving,
    // that means "test" means the same as "TEST" for filenames.
    // if false, the two cases are two different files.
    OCSYNC_EXPORT bool fsCasePreserving();

    // Check if two paths that MUST exist are equal. This function
    // uses QDir::canonicalPath() to judge and cares for the systems
    // case sensitivity.
    OCSYNC_EXPORT bool fileNamesEqual(const QString &fn1, const QString &fn2);

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
     * If the second parameter is omitted, the current time is used.
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
        [[nodiscard]] QDateTime startTime() const;
        [[nodiscard]] QDateTime timeOfLap(const QString &lapName) const;
        [[nodiscard]] quint64 durationOfLap(const QString &lapName) const;
    };

    /**
     * @brief Sort a QStringList in a way that's appropriate for filenames
     */
    OCSYNC_EXPORT void sortFilenames(QStringList &fileNames);

    /** Appends concatPath and queryItems to the url */
    OCSYNC_EXPORT QUrl concatUrlPath(
        const QUrl &url, const QString &concatPath,
        const QUrlQuery &queryItems = {});

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

    OCSYNC_EXPORT QString makeCaseClashConflictFileName(const QString &filename, const QDateTime &datetime);

    /** Returns whether a file name indicates a conflict file
     */
    bool isConflictFile(const char *name) = delete;
    OCSYNC_EXPORT bool isConflictFile(const QString &name);
    OCSYNC_EXPORT bool isCaseClashConflictFile(const QString &name);

    /** Find the base name for a conflict file name, using name pattern only
     *
     * Will return an empty string if it's not a conflict file.
     *
     * Prefer to use the data from the conflicts table in the journal to determine
     * a conflict's base file, see SyncJournal::conflictFileBaseName()
     */
    OCSYNC_EXPORT QByteArray conflictFileBaseNameFromPattern(const QByteArray &conflictName);

    /**
     * @brief Check whether the path is a root of a Windows drive partition ([c:/, d:/, e:/, etc.)
     */
    OCSYNC_EXPORT bool isPathWindowsDrivePartitionRoot(const QString &path);

    /**
     * @brief Retrieves current logged-in user name from the OS
     */
    OCSYNC_EXPORT QString getCurrentUserName();

    /**
     * @brief Registers the desktop app as a handler for a custom URI to enable local editing
     */
    OCSYNC_EXPORT void registerUriHandlerForLocalEditing();
    
    OCSYNC_EXPORT QString leadingSlashPath(const QString &path);
    OCSYNC_EXPORT QString trailingSlashPath(const QString &path);
    OCSYNC_EXPORT QString noLeadingSlashPath(const QString &path);
    OCSYNC_EXPORT QString noTrailingSlashPath(const QString &path);
    OCSYNC_EXPORT QString fullRemotePathToRemoteSyncRootRelative(const QString &fullRemotePath, const QString &remoteSyncRoot);

#ifdef Q_OS_WIN
    OCSYNC_EXPORT bool registryKeyExists(HKEY hRootKey, const QString &subKey);
    OCSYNC_EXPORT QVariant registryGetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName);
    OCSYNC_EXPORT bool registrySetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName, DWORD type, const QVariant &value);
    OCSYNC_EXPORT bool registryDeleteKeyTree(HKEY hRootKey, const QString &subKey);
    OCSYNC_EXPORT bool registryDeleteKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName);
    OCSYNC_EXPORT bool registryWalkSubKeys(HKEY hRootKey, const QString &subKey, const std::function<void(HKEY, const QString &)> &callback);
    OCSYNC_EXPORT bool registryWalkValues(HKEY hRootKey, const QString &subKey, const std::function<void(const QString &, bool *)> &callback);
    OCSYNC_EXPORT QRect getTaskbarDimensions();

    OCSYNC_EXPORT void UnixTimeToLargeIntegerFiletime(time_t t, LARGE_INTEGER *hundredNSecs);

    OCSYNC_EXPORT QString formatWinError(long error);

    OCSYNC_EXPORT bool canCreateFileInPath(const QString &path);

    class OCSYNC_EXPORT NtfsPermissionLookupRAII
    {
    public:
        /**
         * NTFS permissions lookup is disabled by default for performance reasons
         * Enable it and disable it again once we leave the scope
         * https://doc.qt.io/Qt-5/qfileinfo.html#ntfs-permissions
         */
        NtfsPermissionLookupRAII();
        ~NtfsPermissionLookupRAII();

    private:
        Q_DISABLE_COPY(NtfsPermissionLookupRAII);
    };

    /**
     * Closes a Win32 HANDLE if the HANDLE is valid (i.e. not `INVALID_HANDLE_VALUE`).
     */
    struct OCSYNC_EXPORT HandleDeleter {
        typedef HANDLE pointer; // HANDLEs are not really pointers even though they're treated as such

        void operator()(HANDLE handle) const;
    };

    /**
     * A `std::unique_ptr` that automatically closes a HANDLE.
     */
    using UniqueHandle = std::unique_ptr<HANDLE, HandleDeleter>;

    /**
     * Releases a pointer previously allocated by `LocalAlloc`.
     */
    struct OCSYNC_EXPORT LocalFreeDeleter {
        void operator()(void *p) const;
    };

    /**
     * A `std::unique_ptr` that automatically cleans up `P*` types (e.g. `PSID`).
     *
     * Use this whenever the Win32 API docs of a given function tell you to free a returned buffer
     * by calling the `LocalFree` function.
     */
    template<typename T>
    using UniqueLocalFree = std::unique_ptr<typename std::remove_pointer<T>::type, LocalFreeDeleter>;
#endif
}
/** @} */ // \addtogroup

inline constexpr bool Utility::isWindows()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

inline constexpr bool Utility::isMac()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

inline constexpr bool Utility::isUnix()
{
#ifdef Q_OS_UNIX
    return true;
#else
    return false;
#endif
}

inline constexpr bool Utility::isLinux()
{
#if defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

inline constexpr bool Utility::isBSD()
{
#if defined(Q_OS_FREEBSD) || defined(Q_OS_NETBSD) || defined(Q_OS_OPENBSD)
    return true;
#else
    return false;
#endif
}
}
#endif // UTILITY_H
