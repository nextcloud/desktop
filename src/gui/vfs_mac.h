#ifndef LBCONTROLLER
#define LBCONTROLLER
#define FUSE_USE_VERSION 26
#include<QObject>
#include<QStringList>
#include "accountstate.h"

#include <fuse.h>
#include <fuse/fuse_lowlevel.h>

#include <QMutex>
#include <QWaitCondition>
#include "syncwrapper.h"

const QString  kGMUserFileSystemContextUserIDKey = "kGMUserFileSystemContextUserIDKey";
const QString  kGMUserFileSystemContextGroupIDKey = "kGMUserFileSystemContextGroupIDKey";
const QString  kGMUserFileSystemContextProcessIDKey = "kGMUserFileSystemContextProcessIDKey";

const QString  kGMUserFileSystemMountPathKey = "mountPath";

// Attribute keys
const QString  kGMUserFileSystemFileFlagsKey = "kGMUserFileSystemFileFlagsKey";
const QString  kGMUserFileSystemFileAccessDateKey = "kGMUserFileSystemFileAccessDateKey";
const QString  kGMUserFileSystemFileChangeDateKey = "kGMUserFileSystemFileChangeDateKey";
const QString  kGMUserFileSystemFileBackupDateKey = "kGMUserFileSystemFileBackupDateKey";
const QString  kGMUserFileSystemFileSizeInBlocksKey = "kGMUserFileSystemFileSizeInBlocksKey";
const QString  kGMUserFileSystemFileOptimalIOSizeKey = "kGMUserFileSystemFileOptimalIOSizeKey";
const QString  kGMUserFileSystemVolumeSupportsAllocateKey = "kGMUserFileSystemVolumeSupportsAllocateKey";
const QString  kGMUserFileSystemVolumeSupportsCaseSensitiveNamesKey = "kGMUserFileSystemVolumeSupportsCaseSensitiveNamesKey";
const QString  kGMUserFileSystemVolumeSupportsExchangeDataKey = "kGMUserFileSystemVolumeSupportsExchangeDataKey";
const QString  kGMUserFileSystemVolumeSupportsExtendedDatesKey = "kGMUserFileSystemVolumeSupportsExtendedDatesKey";
const QString  kGMUserFileSystemVolumeMaxFilenameLengthKey = "kGMUserFileSystemVolumeMaxFilenameLengthKey";
const QString  kGMUserFileSystemVolumeFileSystemBlockSizeKey = "kGMUserFileSystemVolumeFileSystemBlockSizeKey";
const QString  kGMUserFileSystemVolumeSupportsSetVolumeNameKey = "kGMUserFileSystemVolumeSupportsSetVolumeNameKey";
const QString  kGMUserFileSystemVolumeNameKey = "kGMUserFileSystemVolumeNameKey";

// FinderInfo and ResourceFork keys
const QString kGMUserFileSystemFinderFlagsKey = "kGMUserFileSystemFinderFlagsKey";
const QString kGMUserFileSystemFinderExtendedFlagsKey = "kGMUserFileSystemFinderExtendedFlagsKey";
const QString kGMUserFileSystemCustomIconDataKey = "kGMUserFileSystemCustomIconDataKey";
const QString kGMUserFileSystemWeblocURLKey = "kGMUserFileSystemWeblocURLKey";

static const double kNanoSecondsPerSecond = 1000000000.0;

namespace OCC
{
    struct DiscoveryDirectoryResult;
    class DiscoveryFolderFileList;
}

class InternalVfsMac;

/*!
 * class
 * discussion This class controls the life cycle of a user space file system.
 * The LoopbackController serve file system operations.
 *
 * After instantiating a LoopbackController, call
 * mountAtPath to mount the file system. A call to unmount or an
 * external umount operation will unmount the file system. In addition, the
 * LoopbackController class will post mount and unmount notifications to the
 * default notification center. Since the underlying LoopbackController
 * implementation is multi-threaded, you should assume that notifications will
 * not be posted on the main thread. The object will always be the
 * LoopbackController* and the userInfo will always contain at least the
 * mountPath.<br>
 *
 */                   
class VfsMac : public QObject
{
    Q_OBJECT
    
private:
    QSharedPointer<InternalVfsMac> internal_;
    QString rootPath_;
    qint64 usedQuota_;
    qint64 totalQuota_;
    QMap<QString, OCC::DiscoveryDirectoryResult*> _fileListMap;
    QPointer<OCC::DiscoveryFolderFileList> _remotefileListJob;
    
    QPointer<OCC::AccountState> accountState_;
    int _counter = 0;

	// To sync
    OCC::SyncWrapper *_syncWrapper;
    QMutex _mutex;
    QWaitCondition _syncCondition;
    QWaitCondition _dirCondition;

#pragma mark Fuse operations.
    void mount(QVariantMap args);
    void waitUntilMounted (int fileDescriptor);
    
    /*    QVariantMap finderAttributesAtPath(QString path);
     QVariantMap resourceAttributesAtPath(QString path);
     
     bool hasCustomIconAtPath(QString path);
     bool isDirectoryIconAtPath(QString path, QString *dirPath);*/
    //QByteArray finderDataForAttributes(QVariantMap attributes);
    //QByteArray resourceDataForAttributes(QVariantMap attributes);
    
    QVariantMap defaultAttributesOfItemAtPath(QString path, QVariant userData, QVariantMap &error);
    
public:
    
    typedef enum {
        GMUserFileSystem_NOT_MOUNTED,     // Not mounted.
        GMUserFileSystem_MOUNTING,        // In the process of mounting.
        GMUserFileSystem_INITIALIZING,    // Almost done mounting.
        GMUserFileSystem_MOUNTED,         // Confirmed to be mounted.
        GMUserFileSystem_UNMOUNTING,      // In the process of unmounting.
        GMUserFileSystem_FAILURE,         // Failed state; probably a mount failure.
    } GMUserFileSystemStatus;
    
    typedef enum {
        // Unable to unmount a dead FUSE files system located at mount point.
        GMUserFileSystem_ERROR_UNMOUNT_DEADFS = 1000,
        
        // Gave up waiting for system removal of existing dir in /Volumes/x after
        // unmounting a dead FUSE file system.
        GMUserFileSystem_ERROR_UNMOUNT_DEADFS_RMDIR = 1001,
        
        // The mount point did not exist, and we were unable to mkdir it.
        GMUserFileSystem_ERROR_MOUNT_MKDIR = 1002,
        
        // fuse_main returned while trying to mount and don't know why.
        GMUserFileSystem_ERROR_MOUNT_FUSE_MAIN_INTERNAL = 1003,
    } GMUserFileSystemErrorCode;
    
    /*!
     * abstract Initialize the user space file system.
     * discussion You should only specify YES
     * for isThreadSafe if your file system is thread safe with respect to
     * file system operations. That implies that it implements proper file system
     * locking so that multiple operations on the same file can be done safely.
     * param parent Parent object.
     * param isThreadSafe Is the file system thread safe?
     * result A LoopbackController instance.
     */
    explicit VfsMac(QString rootPath, bool isThreadSafe, OCC::AccountState *accountState, QObject *parent=0);
    
    // The file system for the current thread. Valid only during a FUSE callback.
    static VfsMac* currentFS() {
        struct fuse_context* context = fuse_get_context();
        Q_ASSERT(context);
        return (VfsMac *)context->private_data;
    }
    
    // Convenience method to creates an autoreleased NSError in the
    // NSPOSIXErrorDomain. Filesystem errors returned by the delegate must be
    // standard posix errno values.
    static QVariantMap errorWithCode(int code);
    
    QVariantMap errorWithPosixCode(int code);
    
    /*!
     * abstract Returns the context of the current file system operation.
     * discussion The context of the current file system operation is only valid
     * during a file system callback. The returned dictionary contains the
     * following keys (you must ignore unknown keys):<ul>
     *   <li>kGMUserFileSystemContextUserIDKey
     *   <li>kGMUserFileSystemContextGroupIDKey
     *   <li>kGMUserFileSystemContextProcessIDKey</ul>
     * result The current file system operation context or nil.
     */
    QVariantMap currentContext();
    
    /*!
     * abstract Mount the file system at the given path.
     * discussion Mounts the file system at mountPath with the given set of options.
     * The set of available options can be found on the options wiki page.
     * For example, to turn on debug output add "debug" to the options list.
     * If the mount succeeds, then a FuseFileSystemDidMount signal is sent
     * to the default noification center. If the mount fails, then a
     * FuseFileSystemMountFailed notification will be posted instead.
     * param mountPath The path to mount on, e.g. /Volumes/MyFileSystem
     * param options The set of mount time options to use.
     */
    void mountAtPath(QString mountPath, QStringList options);
    
    /*!
     * abstract Mount the file system at the given path with advanced options.
     * discussion Mounts the file system at mountPath with the given set of options.
     * This is an advanced version of link mountAtPath /link
     * You can use this to mount from a command-line program as follows:<ul>
     *  <li>For an app, use: shouldForeground=YES, detachNewThread=YES
     *  <li>For a daemon: shouldForeground=NO, detachNewThread=NO
     *  <li>For debug output: shouldForeground=YES, detachNewThread=NO
     *  <li>For a daemon+runloop:  shouldForeground=NO, detachNewThread=YES<br>
     *    - Note: I've never tried daemon+runloop; maybe it doesn't make sense</ul>
     * param mountPath The path to mount on, e.g. /Volumes/MyFileSystem
     * param options The set of mount time options to use.
     * param shouldForeground Should the file system thread remain foreground rather
     *        than daemonize? (Recommend: YES)
     * param detachNewThread Should the file system run in a new thread rather than
     *        the current one? (Recommend: YES)
     */
    void mountAtPath(QString mountPath, QStringList options, bool shouldForeground, bool detachNewThread);
    
    /*!
     * abstract Unmount the file system.
     * discussion Unmounts the file system. The FuseFileSystemDidUnmount
     * notification will be posted.
     */
    void unmount();
    
    /*!
     * abstract Invalidate caches and post file system event.
     * discussion Invalidate caches for the specified path and post a file system
     * event to notify subscribed processes, e.g. Finder, of remote file changes.
     * param path The path to the specified file.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the caches were successfully invalidated.
     */
    bool invalidateItemAtPath(QString path, QVariantMap &error);
    
#pragma mark Directory Contents
    
    /*!
     * abstract Returns directory contents at the specified path.
     * discussion Returns an array of NSString containing the names of files and
     * sub-directories in the specified directory.
     * seealso man readdir(3)
     * param path The path to a directory.
     * param error Should be filled with a POSIX error in case of failure.
     * result A QStringList or nil on error.
     */
    QStringList* contentsOfDirectoryAtPath(QString path, QVariantMap &error);
    
#pragma mark Getting and Setting Attributes
    
    /*!
     * abstract Returns file system attributes.
     * discussion
     * Returns a dictionary of attributes for the file system.
     * The following keys are currently supported (unknown keys are ignored):<ul>
     *   <li>NSFileSystemSize
     *   <li>NSFileSystemFreeSize
     *   <li>NSFileSystemNodes
     *   <li>NSFileSystemFreeNodes
     *   <li>kGMUserFileSystemVolumeSupportsExtendedDatesKey
     *   <li>kGMUserFileSystemVolumeMaxFilenameLengthKey
     *   <li>kGMUserFileSystemVolumeFileSystemBlockSizeKey</ul>
     *   <li>kGMUserFileSystemVolumeSupportsCaseSensitiveNamesKey</ul>
     *
     * seealso man statvfs(3)
     * param path A path on the file system (it is safe to ignore this).
     * param error Should be filled with a POSIX error in case of failure.
     * result A dictionary of attributes for the file system.
     */
    QVariantMap attributesOfFileSystemForPath(QString path, QVariantMap &error);
    
    /*!
     * abstract Set attributes at the specified path.
     * discussion
     * Sets the attributes for the item at the specified path. The following keys
     * may be present (you must ignore unknown keys):<ul>
     *   <li>NSFileSize
     *   <li>NSFileOwnerAccountID
     *   <li>NSFileGroupOwnerAccountID
     *   <li>NSFilePosixPermissions
     *   <li>NSFileModificationDate
     *   <li>NSFileCreationDate                  (if supports extended dates)
     *   <li>kGMUserFileSystemFileBackupDateKey  (if supports extended dates)
     *   <li>kGMUserFileSystemFileChangeDateKey
     *   <li>kGMUserFileSystemFileAccessDateKey
     *   <li>kGMUserFileSystemFileFlagsKey</ul>
     *
     * If this is the f-variant and userData was supplied in openFileAtPath: or
     * createFileAtPath: then it will be passed back in this call.
     *
     * seealso man truncate(2), chown(2), chmod(2), utimes(2), chflags(2),
     *              ftruncate(2), fchown(2), fchmod(2), futimes(2), fchflags(2)
     * param attributes The attributes to set.
     * param path The path to the item.
     * param userData The userData corresponding to this open file or nil.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the attributes are successfully set.
     */
    bool setAttributes(QVariantMap attributes, QString path, QVariant userInfo, QVariantMap &error);
    
#pragma mark File Contents

    /*
    * abstract Get Process name by Id.
    * discussion Just returns proccess name.
    * param pid
    *
    */
   char *getProcessName(pid_t pid);
    
    /*!
     * abstract Opens the file at the given path for read/write.
     * discussion This will only be called for existing files. If the file needs
     * to be created then createFileAtPath: will be called instead.
     * seealso man open(2)
     * param path The path to the file.
     * param mode The open mode for the file (e.g. O_RDWR, etc.)
     * param userData Out parameter that can be filled in with arbitrary user data.
     *        The given userData will be retained and passed back in to delegate
     *        methods that are acting on this open file.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the file was opened successfully.
     */
    bool openFileAtPath(QString path, int mode, QVariant &userData, QVariantMap &error);
    
    /*!
     * abstract Called when an opened file is closed.
     * discussion If userData was provided in the corresponding openFileAtPath: call
     * then it will be passed in userData and released after this call completes.
     * seealso man close(2)
     * param path The path to the file.
     * param userData The userData corresponding to this open file or nil.
     */
    void releaseFileAtPath(QString path, QVariant userData);
    
    /*!
     * abstract Reads data from the open file at the specified path.
     * discussion Reads data from the file starting at offset into the provided
     * buffer and returns the number of bytes read. If userData was provided in the
     * corresponding openFileAtPath: or createFileAtPath: call then it will be
     * passed in.
     * seealso man pread(2)
     * param path The path to the file.
     * param userData The userData corresponding to this open file or nil.
     * param buffer Byte buffer to read data from the file into.
     * param size The size of the provided buffer.
     * param offset The offset in the file from which to read data.
     * param error Should be filled with a POSIX error in case of failure.
     * result The number of bytes read or -1 on error.
     */
    int readFileAtPath(QString path, QVariant userData, char *buffer, size_t size, off_t offset, QVariantMap &error);
    
    /*!
     * abstract Writes data to the open file at the specified path.
     * discussion Writes data to the file starting at offset from the provided
     * buffer and returns the number of bytes written. If userData was provided in
     * the corresponding openFileAtPath: or createFileAtPath: call then it will be
     * passed in.
     * seealso man pwrite(2)
     * param path The path to the file.
     * param userData The userData corresponding to this open file or nil.
     * param buffer Byte buffer containing the data to write to the file.
     * param size The size of the provided buffer.
     * param offset The offset in the file to write data.
     * param error Should be filled with a POSIX error in case of failure.
     * result The number of bytes written or -1 on error.
     */
    int writeFileAtPath(QString path, QVariant userData, const char *buffer, size_t size, off_t offset, QVariantMap &error);
    
    /*!
     * abstract Preallocates space for the open file at the specified path.
     * discussion Preallocates file storage space. Upon success, the space that is
     * allocated can be the same size or larger than the space requested and
     * subsequent writes to bytes in the specified range are guaranteed not to fail
     * because of lack of disk space.
     *
     * If userData was provided in the corresponding openFileAtPath: or
     * createFileAtPath: call then it will be passed in.
     *
     * seealso man fcntl(2)
     * param path The path to the file.
     * param userData The userData corresponding to this open file or nil.
     * param options The preallocate options. See <sys/vnode.h>.
     * param offset The start of the region.
     * param length The size of the region.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the space was preallocated successfully.
     */
    bool preallocateFileAtPath(QString path, QVariant userData, int options, off_t offset, off_t length, QVariantMap &error);
    
    /*!
     * abstract Atomically exchanges data between files.
     * discussion  Called to atomically exchange file data between path1 and path2.
     * seealso man exchangedata(2)
     * param path1 The path to the file.
     * param path2 The path to the other file.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if data was exchanged successfully.
     */
    bool exchangeDataOfItemAtPath(QString path1, QString path2, QVariantMap &error);
    
#pragma mark Creating an Item
    
    /*!
     * abstract Creates a directory at the specified path.
     * discussion  The attributes may contain keys similar to setAttributes:.
     * seealso man mkdir(2)
     * param path The directory path to create.
     * param attributes Set of attributes to apply to the newly created directory.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the directory was successfully created.
     */
    bool createDirectoryAtPath(QString path, QVariantMap attributes, QVariantMap &error);
    
    /*!
     * abstract Creates and opens a file at the specified path.
     * discussion  This should create and open the file at the same time. The
     * attributes may contain keys similar to setAttributes:.
     * seealso man open(2)
     * param path The path of the file to create.
     * param attributes Set of attributes to apply to the newly created file.
     * param flags Open flags (see open man page)
     * param userData Out parameter that can be filled in with arbitrary user data.
     *        The given userData will be retained and passed back in to delegate
     *        methods that are acting on this open file.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the directory was successfully created.
     */
    bool createFileAtPath(QString path, QVariantMap attributes, int flags, QVariant &userData, QVariantMap &error);
    
#pragma mark Moving an Item
    
    /*!
     * abstract Moves or renames an item.
     * discussion Move, also known as rename, is one of the more difficult file
     * system methods to implement properly. Care should be taken to handle all
     * error conditions and return proper POSIX error codes.
     * seealso man rename(2)
     * param source The source file or directory.
     * param destination The destination file or directory.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the move was successful.
     */
    bool moveItemAtPath(QString source, QString destination, QVariantMap &error);
    
#pragma mark Removing an Item
    
    /*!
     * abstract Remove the directory at the given path.
     * discussion Unlike NSFileManager, this should not recursively remove
     * subdirectories. If this method is not implemented, then removeItemAtPath
     * will be called even for directories.
     * seealso man rmdir(2)
     * param path The directory to remove.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the directory was successfully removed.
     */
    bool removeDirectoryAtPath(QString path, QVariantMap &error);
    
    /*!
     * abstract Removes the item at the given path.
     * discussion This should not recursively remove subdirectories. If
     * removeDirectoryAtPath is implemented, then that will be called instead of
     * this selector if the item is a directory.
     * seealso man unlink(2), rmdir(2)
     * param path The path to the item to remove.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the item was successfully removed.
     */
    bool removeItemAtPath(QString path, QVariantMap &error);
    
#pragma mark Linking an Item
    
    /*!
     * @abstract Creates a hard link.
     * @seealso man link(2)
     * @param path The path for the created hard link.
     * @param otherPath The path that is the target of the created hard link.
     * @param error Should be filled with a POSIX error in case of failure.
     * @result YES if the hard link was successfully created.
     */
    bool linkItemAtPath(QString path, QString otherPath, QVariantMap &error);
    
#pragma mark Symbolic Links
    
    /*!
     * @abstract Creates a symbolic link.
     * @seealso man symlink(2)
     * @param path The path for the created symbolic link.
     * @param otherPath The path that is the target of the symbolic link.
     * @param error Should be filled with a POSIX error in case of failure.
     * @result YES if the symbolic link was successfully created.
     */
    bool createSymbolicLinkAtPath(QString path, QString otherPath, QVariantMap &error);
    
    /*!
     * @abstract Reads the destination of a symbolic link.
     * @seealso man readlink(2)
     * @param path The path to the specified symlink.
     * @param error Should be filled with a POSIX error in case of failure.
     * @result The destination path of the symbolic link or nil on error.
     */
    QString destinationOfSymbolicLinkAtPath(QString path, QVariantMap &error);
    
#pragma mark Extended Attributes
    
    /*!
     * abstract Returns the names of the extended attributes at the specified path.
     * discussion If there are no extended attributes at this path, then return an
     * empty array. Return nil only on error.
     * seealso man listxattr(2)
     * param path The path to the specified file.
     * param error Should be filled with a POSIX error in case of failure.
     * result A List of extended attribute names or nil on error.
     */
    QStringList* extendedAttributesOfItemAtPath(QString path, QVariantMap &error);
    
    /*!
     * abstract Returns the contents of the extended attribute at the specified path.
     * seealso man getxattr(2)
     * param name The name of the extended attribute.
     * param path The path to the specified file.
     * param position The offset within the attribute to read from.
     * param error Should be filled with a POSIX error in case of failure.
     * result The data corresponding to the attribute or nil on error.
     */
    QByteArray* valueOfExtendedAttribute(QString name, QString path, off_t position, QVariantMap &error);
    
    /*!
     * abstract Writes the contents of the extended attribute at the specified path.
     * seealso man setxattr(2)
     * param name The name of the extended attribute.
     * param path The path to the specified file.
     * param value The data to write.
     * param position The offset within the attribute to write to
     * param options Options (see setxattr man page).
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the attribute was successfully written.
     */
    bool setExtendedAttribute(QString name, QString path, QByteArray value, off_t position, int options, QVariantMap &error);
    
    /*!
     * abstract Removes the extended attribute at the specified path.
     * seealso man removexattr(2)
     * param name The name of the extended attribute.
     * param path The path to the specified file.
     * param error Should be filled with a POSIX error in case of failure.
     * result YES if the attribute was successfully removed.
     */
    bool removeExtendedAttribute(QString name, QString path, QVariantMap &error);

    //~VfsMac();
    
    bool enableAllocate();
    bool enableCaseSensitiveNames();
    bool enableExchangeData();
    bool enableExtendedTimes();
    bool enableSetVolumeName();
    void fuseInit();
    void fuseDestroy();
    
    bool fillStatfsBuffer(struct statfs *stbuf, QString path, QVariantMap &error);
    bool fillStatBuffer(struct stat *stbuf, QString path, QVariant userData, QVariantMap &error);
    QVariantMap *extendedTimesOfItemAtPath(QString path, QVariant userData, QVariantMap &error);
    
    void setTotalQuota(qint64 totalQuota) { totalQuota_ = totalQuota; }
    void setUsedQuota(qint64 usedQuota) { usedQuota_ = usedQuota; }
    
    qint64 totalQuota() { return totalQuota_; }
    qint64 usedQuota() { return usedQuota_; }
    
public slots:
    void folderFileListFinish(OCC::DiscoveryDirectoryResult *dr);

    // To sync: notify syncing is done
    void slotSyncFinish();

signals:
    void FuseFileSystemDidMount(QVariantMap userInfo);
    void FuseFileSystemMountFailed(QVariantMap error);
    void FuseFileSystemDidUnmount(QVariantMap userInfo);
    void startRemoteFileListJob(QString path);

    // To sync: propagate FUSE operations to the sync engine
    void openFile(const QString path);
    void writeFile(const QString path);
    void deleteFile(const QString path);
};

#endif
