/*
 * Copyright (C) 2018 by AMCO
 * Copyright (C) 2018 by Jesús Deloya <jdeloya_ext@amco.mx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "vfs_mac.h"
#include "fileManager.h"
#include "discoveryphase.h"

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/vnode.h>
#include <sys/xattr.h>
#include <libproc.h>

#include <QtCore>
#include <thread>

#include <sys/ioctl.h>

#include "syncwrapper.h"

class InternalVfsMac : public QObject
{
private:
    struct fuse* handle_;
    QString mountPath_;
    VfsMac::GMUserFileSystemStatus status_;
    bool shouldCheckForResource_;     // Try to handle FinderInfo/Resource Forks?
    bool isThreadSafe_;               // Is the delegate thread-safe?
    bool supportsAllocate_;           // Delegate supports preallocation of files?
    bool supportsCaseSensitiveNames_; // Delegate supports case sensitive names?
    bool supportsExchangeData_;       // Delegate supports exchange data?
    bool supportsExtendedTimes_;      // Delegate supports create and backup times?
    bool supportsSetVolumeName_;      // Delegate supports setvolname?
    bool isReadOnly_;                 // Is this mounted read-only?
    
public:
    explicit InternalVfsMac(QObject *parent = 0, bool isThreadSafe=false)
                    : QObject(parent)
    {
        status_ = VfsMac::GMUserFileSystem_NOT_MOUNTED;
        isThreadSafe_ = isThreadSafe;
        supportsAllocate_ = false;
        supportsCaseSensitiveNames_ = true;
        supportsExchangeData_ = false;
        supportsExtendedTimes_ = true;
        supportsSetVolumeName_ = false;
        isReadOnly_ = false;
        shouldCheckForResource_=false;
    }
    struct fuse* handle () { return handle_; }
    void setHandle (struct fuse *handle) { handle_ = handle; }
    QString mountPath () { return mountPath_; }
    void setMountPath (QString mountPath) { mountPath_ = mountPath;}
    VfsMac::GMUserFileSystemStatus status ()
    {
        return this->status_;
    }
    void setStatus (VfsMac::GMUserFileSystemStatus status) { this->status_ = status; }
    bool isThreadSafe () { return isThreadSafe_; }
    bool supportsAllocate () { return supportsAllocate_; };
    void setSupportsAllocate (bool val) { supportsAllocate_ = val; }
    bool supportsCaseSensitiveNames () { return supportsCaseSensitiveNames_; }
    void setSupportsCaseSensitiveNames (bool val) { supportsCaseSensitiveNames_ = val; }
    bool supportsExchangeData () { return supportsExchangeData_; }
    void setSupportsExchangeData (bool val) { supportsExchangeData_ = val; }
    bool supportsExtendedTimes () { return supportsExtendedTimes_; }
    void setSupportsExtendedTimes (bool val) { supportsExtendedTimes_ = val; }
    bool supportsSetVolumeName () { return supportsSetVolumeName_; }
    void setSupportsSetVolumeName (bool val) { supportsSetVolumeName_ = val; }
    bool shouldCheckForResource () { return shouldCheckForResource_; }
    bool isReadOnly () { return isReadOnly_; }
    void setIsReadOnly (bool val) { isReadOnly_ = val; }
    ~InternalVfsMac() {  }
    
};

VfsMac::VfsMac(QString rootPath, bool isThreadSafe, OCC::AccountState *accountState, QObject *parent)
    :QObject(parent)
    , internal_(new InternalVfsMac(parent, isThreadSafe))
    , accountState_(accountState)
{
    rootPath_ = rootPath;
    totalQuota_ = (2LL * 1024 * 1024 * 1024);
    usedQuota_ = 0;
    _remotefileListJob = new OCC::DiscoveryFolderFileList(accountState_->account());
    _remotefileListJob->setParent(this);
    connect(this, &VfsMac::startRemoteFileListJob, _remotefileListJob, &OCC::DiscoveryFolderFileList::doGetFolderContent);
    connect(_remotefileListJob, &OCC::DiscoveryFolderFileList::gotDataSignal, this, &VfsMac::folderFileListFinish);
}

bool VfsMac::enableAllocate() {
    return internal_->supportsAllocate();
}
bool VfsMac::enableCaseSensitiveNames() {
    return internal_->supportsCaseSensitiveNames();
}
bool VfsMac::enableExchangeData() {
    return internal_->supportsExchangeData();
}
bool VfsMac::enableExtendedTimes() {
    return internal_->supportsExtendedTimes();
}
bool VfsMac::enableSetVolumeName() {
    return internal_->supportsSetVolumeName();
}

QVariantMap VfsMac::currentContext()
{
    struct fuse_context* context = fuse_get_context();
    QVariantMap dict;
    if (!context) {
        return dict;
    }

    dict.insert(kGMUserFileSystemContextUserIDKey, QVariant((unsigned int) context->uid));
    dict.insert(kGMUserFileSystemContextGroupIDKey, QVariant((unsigned int) context->gid));
    dict.insert(kGMUserFileSystemContextProcessIDKey, QVariant((unsigned int) context->pid));

    return dict;
}

void VfsMac::mountAtPath(QString mountPath, QStringList options)
{
    this->mountAtPath(mountPath, options, true, true);
}

void VfsMac::mountAtPath(QString mountPath, QStringList options, bool shouldForeground, bool detachNewThread)
{
    internal_->setMountPath(mountPath);
    QStringList optionCopy;
    foreach(const QString option, options)
    {
        QString optionLower = option.toLower();
        if (optionLower == "rdonly" ||
            optionLower == "ro") {
            internal_->setIsReadOnly(true);
        }
        optionCopy.append(option);
    }
    QVariantMap args;
    args.insert("options", optionCopy);
    args.insert("shouldForeground", shouldForeground);
    if (detachNewThread) {
        std::thread t(&VfsMac::mount, this, args);
        t.detach();
    } else {
        this->mount(args);
    }
}

void VfsMac::unmount() {
    if (internal_.data() != nullptr && internal_->status() == GMUserFileSystem_MOUNTED) {
        QStringList args;
        args << "-v" << internal_->mountPath();
        QProcess *defaultsProcess = new QProcess();
        defaultsProcess->start("/sbin/umount", args);
        defaultsProcess->waitForFinished();
        defaultsProcess->close();
        defaultsProcess->deleteLater();
    }
}

bool VfsMac::invalidateItemAtPath(QString path, QVariantMap &error)
{
    int ret = -ENOTCONN;
    
    struct fuse* handle = internal_->handle();
    if (handle) {
        ret = fuse_invalidate(handle, path.toLatin1().data());
        
        // Note: fuse_invalidate_path() may return -ENOENT to indicate that there
        // was no entry to be invalidated, e.g., because the path has not been seen
        // before or has been forgotten. This should not be considered to be an
        // error.
        if (ret == -ENOENT) {
            ret = 0;
        }
    }
    if (ret != 0)
    {
        error = VfsMac::errorWithCode(ret);
        return false;
    }
    return true;
}


QVariantMap VfsMac::errorWithCode(int code)
{
    QVariantMap error;
    error.insert("code", code);
    error.insert("localizedDescription", strerror(code));
    return error;
}

QVariantMap VfsMac::errorWithPosixCode(int code)
{
    QVariantMap error;
    error.insert("code", code);
    error.insert("domain", FileManager::FMPOSIXErrorDomain);
    error.insert("localizedDescription", strerror(code));
    return error;
}

#define FUSEDEVIOCGETHANDSHAKECOMPLETE _IOR('F', 2, u_int32_t)
static const int kMaxWaitForMountTries = 50;
static const int kWaitForMountUSleepInterval = 100000;  // 100 ms

void VfsMac::waitUntilMounted (int fileDescriptor)
{
    for (int i = 0; i < kMaxWaitForMountTries; ++i) {
        u_int32_t handShakeComplete = 0;
        int ret = ioctl(fileDescriptor, FUSEDEVIOCGETHANDSHAKECOMPLETE,
                        &handShakeComplete);
        if (ret == 0 && handShakeComplete) {
            internal_->setStatus(GMUserFileSystem_MOUNTED);
            
            // Successfully mounted, so post notification.
            QVariantMap userInfo;
            userInfo.insert(kGMUserFileSystemMountPathKey, internal_->mountPath());
            
            emit FuseFileSystemDidMount(userInfo);
            return;
        }
        usleep(kWaitForMountUSleepInterval);
    }
}

void VfsMac::fuseInit()
{
    struct fuse_context* context = fuse_get_context();
    
    internal_->setHandle(context->fuse);
    internal_->setStatus(GMUserFileSystem_INITIALIZING);
    QVariantMap error;
    QVariantMap attribs = this->attributesOfFileSystemForPath("/", error);
    
    if (!attribs.isEmpty()) {
        int supports = 0;
        
        supports = attribs.value(kGMUserFileSystemVolumeSupportsAllocateKey).toInt();
        internal_->setSupportsAllocate(supports);
        
        supports = attribs.value(kGMUserFileSystemVolumeSupportsCaseSensitiveNamesKey).toInt();
        internal_->setSupportsCaseSensitiveNames(supports);
        
        supports = attribs.value(kGMUserFileSystemVolumeSupportsExchangeDataKey).toInt();
        internal_->setSupportsExchangeData(supports);
        
        supports = attribs.value(kGMUserFileSystemVolumeSupportsExtendedDatesKey).toInt();
        internal_->setSupportsExtendedTimes(supports);
        
        supports = attribs.value(kGMUserFileSystemVolumeSupportsSetVolumeNameKey).toInt();
        internal_->setSupportsSetVolumeName(supports);
    }
    
    // The mount point won't actually show up until this winds its way
    // back through the kernel after this routine returns. In order to post
    // the kGMUserFileSystemDidMount notification we start a new thread that will
    // poll until it is mounted.
    struct fuse_session* se = fuse_get_session(context->fuse);
    struct fuse_chan* chan = fuse_session_next_chan(se, NULL);
    int fd = fuse_chan_fd(chan);
    
    std::thread t(&VfsMac::waitUntilMounted, this, fd);
    t.detach();
}

void VfsMac::fuseDestroy()
{
    internal_->setStatus(GMUserFileSystem_UNMOUNTING);
    
    QVariantMap userInfo;
    userInfo.insert(kGMUserFileSystemMountPathKey, internal_->mountPath());
    
    emit FuseFileSystemDidUnmount(userInfo);
}

#pragma mark Internal Stat Operations

bool VfsMac::fillStatfsBuffer(struct statfs *stbuf, QString path, QVariantMap &error)
{
    QVariantMap attributes = this->attributesOfFileSystemForPath(path, error);
    if (attributes.isEmpty()) {
        return false;
    }
    
    // Block size
    Q_ASSERT(attributes.contains(kGMUserFileSystemVolumeFileSystemBlockSizeKey));
    stbuf->f_bsize = (uint32_t)attributes.value(kGMUserFileSystemVolumeFileSystemBlockSizeKey).toUInt();
    stbuf->f_iosize = (int32_t)attributes.value(kGMUserFileSystemVolumeFileSystemBlockSizeKey).toInt();
    
    // Size in blocks
    Q_ASSERT(attributes.contains(FileManager::FMFileSystemSize));
    unsigned long long size = attributes.value(FileManager::FMFileSystemSize).toULongLong();
    stbuf->f_blocks = (uint64_t)(size / stbuf->f_bsize);
    
    // Number of free / available blocks
    Q_ASSERT(attributes.contains(FileManager::FMFileSystemFreeSize));
    unsigned long long freeSize = attributes.value(FileManager::FMFileSystemFreeSize).toULongLong();
    stbuf->f_bavail = stbuf->f_bfree = (uint64_t)(freeSize / stbuf->f_bsize);
    
    // Number of nodes
    Q_ASSERT(attributes.contains(FileManager::FMFileSystemNodes));
    unsigned long long numNodes = attributes.value(FileManager::FMFileSystemNodes).toULongLong();
    stbuf->f_files = (uint64_t)numNodes;
    
    // Number of free / available nodes
    Q_ASSERT(attributes.contains(FileManager::FMFileSystemFreeNodes));
    unsigned long long freeNodes = attributes.value(FileManager::FMFileSystemFreeNodes).toULongLong();
    stbuf->f_ffree = (uint64_t)freeNodes;
    
    return true;
}

bool VfsMac::fillStatBuffer(struct stat *stbuf, QString path, QVariant userData, QVariantMap &error)
{
    QVariantMap attributes = this->defaultAttributesOfItemAtPath(path, userData, error);
    if (attributes.empty()) {
        return false;
    }
    
    // Inode
    if(attributes.contains(FileManager::FMFileSystemFileNumber))
    {
        long long inode = attributes.value(FileManager::FMFileSystemFileNumber).toLongLong();
        stbuf->st_ino = inode;
    }
    
    // Permissions (mode)
    long perm = (long)attributes.value(FileManager::FMFilePosixPermissions).toLongLong();
    stbuf->st_mode = perm;
    QString fileType = attributes.value(FileManager::FMFileType).toString();
    if (fileType == FileManager::FMFileTypeDirectory) {
        stbuf->st_mode |= S_IFDIR;
    } else if (fileType == FileManager::FMFileTypeRegular) {
        stbuf->st_mode |= S_IFREG;
    } else if (fileType == FileManager::FMFileTypeSymbolicLink) {
        stbuf->st_mode |= S_IFLNK;
    } else {
        error = errorWithCode(EFTYPE);
        return false;
    }
    
    // Owner and Group
    // Note that if the owner or group IDs are not specified, the effective
    // user and group IDs for the current process are used as defaults.
    stbuf->st_uid = attributes.contains(FileManager::FMFileOwnerAccountID) ? attributes.value(FileManager::FMFileOwnerAccountID).toLongLong() : geteuid();
    stbuf->st_gid = attributes.contains(FileManager::FMFileGroupOwnerAccountID) ? attributes.value(FileManager::FMFileGroupOwnerAccountID).toLongLong() : getegid();
    
    // nlink
    long nlink = attributes.value(FileManager::FMFileReferenceCount).toLongLong();
    stbuf->st_nlink = nlink;
    
    // flags
    if (attributes.contains(kGMUserFileSystemFileFlagsKey)) {
        long flags = attributes.value(kGMUserFileSystemFileFlagsKey).toLongLong();
        stbuf->st_flags = flags;
    } else {
        // Just in case they tried to use NSFileImmutable or NSFileAppendOnly
        if (attributes.contains(FileManager::FMFileImmutable))
        {
            bool immutableFlag = attributes.value(FileManager::FMFileImmutable).toBool();
            if (immutableFlag)
                stbuf->st_flags |= UF_IMMUTABLE;
                
        }
        if (attributes.contains(FileManager::FMFileAppendOnly))
        {
            bool appendFlag = attributes.value(FileManager::FMFileAppendOnly).toBool();
            if (appendFlag)
                stbuf->st_flags |= UF_APPEND;
        }
    }
    
    // Note: We default atime, ctime to mtime if it is provided.
    if(attributes.contains(FileManager::FMFileModificationDate))
    {
        QDateTime mdate = attributes.value(FileManager::FMFileModificationDate).toDateTime();
        if (mdate.isValid()) {
            const double seconds_dp = mdate.toMSecsSinceEpoch()/1000;
            const time_t t_sec = (time_t) seconds_dp;
            const double nanoseconds_dp = ((seconds_dp - t_sec) * kNanoSecondsPerSecond);
            const long t_nsec = (nanoseconds_dp > 0 ) ? nanoseconds_dp : 0;
            
            stbuf->st_mtimespec.tv_sec = t_sec;
            stbuf->st_mtimespec.tv_nsec = t_nsec;
            stbuf->st_atimespec = stbuf->st_mtimespec;  // Default to mtime
            stbuf->st_ctimespec = stbuf->st_mtimespec;  // Default to mtime
        }
    }
    if(attributes.contains(kGMUserFileSystemFileAccessDateKey))
    {
        QDateTime adate = attributes.value(kGMUserFileSystemFileAccessDateKey).toDateTime();
        if (adate.isValid()) {
            const double seconds_dp = adate.toMSecsSinceEpoch()/1000;
            const time_t t_sec = (time_t) seconds_dp;
            const double nanoseconds_dp = ((seconds_dp - t_sec) * kNanoSecondsPerSecond);
            const long t_nsec = (nanoseconds_dp > 0 ) ? nanoseconds_dp : 0;
            stbuf->st_atimespec.tv_sec = t_sec;
            stbuf->st_atimespec.tv_nsec = t_nsec;
        }
    }
    if(attributes.contains(kGMUserFileSystemFileChangeDateKey))
    {
        QDateTime cdate = attributes.value(kGMUserFileSystemFileChangeDateKey).toDateTime();
        if (cdate.isValid()) {
            const double seconds_dp = cdate.toMSecsSinceEpoch()/1000;
            const time_t t_sec = (time_t) seconds_dp;
            const double nanoseconds_dp = ((seconds_dp - t_sec) * kNanoSecondsPerSecond);
            const long t_nsec = (nanoseconds_dp > 0 ) ? nanoseconds_dp : 0;
            stbuf->st_ctimespec.tv_sec = t_sec;
            stbuf->st_ctimespec.tv_nsec = t_nsec;
        }
    }
    
#ifdef _DARWIN_USE_64_BIT_INODE
    if(attributes.contains(FileManager::FMFileCreationDate))
    {
        QDateTime bdate = attributes.value(FileManager::FMFileCreationDate).toDateTime();
        if (bdate.isValid()) {
            const double seconds_dp = bdate.toMSecsSinceEpoch()/1000;
            const time_t t_sec = (time_t) seconds_dp;
            const double nanoseconds_dp = ((seconds_dp - t_sec) * kNanoSecondsPerSecond);
            const long t_nsec = (nanoseconds_dp > 0 ) ? nanoseconds_dp : 0;
            stbuf->st_birthtimespec.tv_sec = t_sec;
            stbuf->st_birthtimespec.tv_nsec = t_nsec;
        }
    }
#endif
    
    // File size
    // Note that the actual file size of a directory depends on the internal
    // representation of directories in the particular file system. In general
    // this is not the combined size of the files in that directory.
    if(attributes.contains(FileManager::FMFileSize))
    {
        long long size = attributes.value(FileManager::FMFileSize).toLongLong();
        stbuf->st_size = size;
    }
    
    // Set the number of blocks used so that Finder will display size on disk
    // properly. The man page says that this is in terms of 512 byte blocks.
    if (attributes.contains(kGMUserFileSystemFileSizeInBlocksKey))
    {
        long long blocks = attributes.value(kGMUserFileSystemFileSizeInBlocksKey).toLongLong();
        stbuf->st_blocks = blocks;
    }
    else if (stbuf->st_size > 0)
    {
        stbuf->st_blocks = stbuf->st_size / 512;
        if (stbuf->st_size % 512)
            ++(stbuf->st_blocks);
    }
    
    // Optimal file I/O size
    if (attributes.contains(kGMUserFileSystemFileOptimalIOSizeKey))
    {
        int ioSize = attributes.value(kGMUserFileSystemFileOptimalIOSizeKey).toInt();
        stbuf->st_blksize = ioSize;
    }
    
    return true;
}
#pragma mark Creating an Item

bool VfsMac::createDirectoryAtPath(QString path, QVariantMap attributes, QVariantMap &error)
{
    QString p = rootPath_ + path;
    FileManager fm;
    return fm.createDirectory(p, attributes, error);
}

bool VfsMac::createFileAtPath(QString path, QVariantMap attributes, int flags, QVariant &userData, QVariantMap &error)
{
    QString p = rootPath_ + path;
    FileManager fm;
    return fm.createFileAtPath(p, attributes, flags, userData, error);
}

#pragma mark Removing an Item

bool VfsMac::removeDirectoryAtPath(QString path, QVariantMap &error)
{
    QString p = rootPath_ + path;
    int ret = rmdir(p.toLatin1().data());
    if (ret < 0) {
        error = errorWithPosixCode(errno);
        return false;
    }
    return true;
}

bool VfsMac::removeItemAtPath(QString path, QVariantMap &error)
{
    QString p = rootPath_ + path;
    FileManager fs;
    return fs.removeItemAtPath(p, error);
}

#pragma mark Moving an Item

bool VfsMac::moveItemAtPath(QString source, QString destination, QVariantMap &error)
{
    // We use rename directly here since NSFileManager can sometimes fail to
    // rename and return non-posix error codes.
    QString p_src = rootPath_  + source;
    QString p_dst = rootPath_ + destination;
    int ret = rename(p_src.toLatin1().data(), p_dst.toLatin1().data());
    if ( ret < 0 ) {
        error = errorWithPosixCode(errno);
        return false;
    }
    return true;
}

#pragma mark Linking an Item

bool VfsMac::linkItemAtPath(QString path, QString otherPath, QVariantMap &error)
{
    QString p_path = rootPath_ + path;
    QString p_otherPath = rootPath_ + otherPath;
    
    // We use link rather than the NSFileManager equivalent because it will copy
    // the file rather than hard link if part of the root path is a symlink.
    int rc = link(p_path.toLatin1().data(), p_otherPath.toLatin1().data());
    if ( rc <  0 ) {
        error = errorWithPosixCode(errno);
        return false;
    }
    return true;
}

#pragma mark Symbolic Links

bool VfsMac::createSymbolicLinkAtPath(QString path, QString otherPath, QVariantMap &error)
{
    FileManager fm;
    return fm.createSymbolicLinkAtPath(rootPath_ + path, otherPath, error);
}

QString VfsMac::destinationOfSymbolicLinkAtPath(QString path, QVariantMap &error)
{
    FileManager fm;
    return fm.destinationOfSymbolicLinkAtPath(rootPath_ + path, error);
}

#pragma mark Directory Contents

void VfsMac::folderFileListFinish(OCC::DiscoveryDirectoryResult *dr)
{
    if(dr)
    {
        QString ruta = dr->path;
        _fileListMap.insert(dr->path, dr);
    }
    else
        qDebug() << "Error al obtener los resultados, viene nulo";
}

QStringList *VfsMac::contentsOfDirectoryAtPath(QString path, QVariantMap &error)
{
    emit startRemoteFileListJob(path);
    
    while (!_fileListMap.contains(path))
    {
        qDebug() << Q_FUNC_INFO << "looking for " << path << "in: " << _fileListMap.keys();
    }
    
    if(_fileListMap.value(path)->code != 0)
    {
        errorWithPosixCode(_fileListMap.value(path)->code);
        return nullptr;
    }
    
    FileManager fm;
    if(!_fileListMap.value(path)->list.empty())
    {
        for(unsigned long i=0; i <_fileListMap.value(path)->list.size(); i++)
        {
            QString completePath = rootPath_ + (path.endsWith("/")?path:(path+"/")) + QString::fromLatin1(_fileListMap.value(path)->list.at(i)->path);
            QFileInfo fi(completePath);
            if (!fi.exists()){
                if(_fileListMap.value(path)->list.at(i)->type == ItemTypeDirectory)
                {
                    unsigned long perm = 16877 & ALLPERMS;
                    QVariantMap attribs;
                    attribs.insert(FileManager::FMFilePosixPermissions, (long long)perm);
                    fm.createDirectory(completePath, attribs, error);
                } else if (_fileListMap.value(path)->list.at(i)->type == ItemTypeFile) {
                    QVariant fd;
                    unsigned long perm = ALLPERMS;
                    QVariantMap attribs;
                    attribs.insert(FileManager::FMFilePosixPermissions, (long long)perm);
                    fm.createFileAtPath(completePath, attribs, fd, error);
                    close(fd.toInt());
                }
            }
            OCC::SyncWrapper::instance()->initSyncMode(_fileListMap.value(path)->list.at(i)->path);
            OCC::SyncWrapper::instance()->updateLocalFileTree(_fileListMap.value(path)->list.at(i)->path);
        }
    }
    //_fileListMap.remove(path);
    
    return new QStringList (fm.contentsOfDirectoryAtPath(rootPath_ + path, error));
}

#pragma mark File Contents

char * VfsMac::getProcessName(pid_t pid)
{
   char pathBuffer[PROC_PIDPATHINFO_MAXSIZE];
   int ret = proc_pidpath(pid, pathBuffer, sizeof(pathBuffer));

   if (ret <= 0) {
       fprintf(stderr, "PID %d: proc_pidpath ();\n", pid);
       fprintf(stderr, "    %s\n", strerror(errno));
   } else {
       printf("proc %d: %s\n", pid, pathBuffer);

   }

   struct proc_bsdinfo info;
   proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info));

   char name[2*MAXCOMLEN];
   proc_name(pid, name, sizeof(name));

   // proc_pidpath(pid, buffer, buffersize)
   char path[PROC_PIDPATHINFO_MAXSIZE];
   proc_pidpath(pid, path, sizeof(path));

   printf("%d\t%d\t%-15s\t%s\tproc_name=%s\tpath=%s\n",
          info.pbi_pid, info.pbi_ppid, info.pbi_comm, info.pbi_name,
          name, path);

   QString path1(QString::fromUtf8(pathBuffer));
   path1 = path1.mid(path1.lastIndexOf("/") + 1);

   char nameBuffer[256];

//   int position = strlen(pathBuffer);
//   while(position >= 0 && pathBuffer[position] != 0xEB/0xED)
//   {
//       position--;
//   }

//   strcpy(nameBuffer, pathBuffer + position + 1);

   strcpy(nameBuffer, path1.toStdString().data());

   return nameBuffer;
}

bool VfsMac::openFileAtPath(QString path, int mode, QVariant &userData, QVariantMap &error)
{
   struct fuse_context *context = fuse_get_context();

   QString nameBuffer = QString::fromLatin1(getProcessName(context->pid));

   qDebug() << "JJDCname: " << nameBuffer;

   if(nameBuffer != "Finder" && nameBuffer != "QuickLookSatellite" && nameBuffer != "")
   {
       qDebug() << "Push here sync algorithm";
       OCC::SyncWrapper::instance()->openFileAtPath(path);
       while(!OCC::SyncWrapper::instance()->syncDone(path))
           qDebug() << "Syncing...";
   }

    QString p = rootPath_ + path;
    int fd = open(p.toLatin1().data(), mode);
    if ( fd < 0 ) {
        error = errorWithPosixCode(errno);
        return false;
    }
    userData = (long long)fd;
    return true;
}

void VfsMac::releaseFileAtPath(QString path, QVariant userData)
{
    OCC::SyncWrapper::instance()->releaseFileAtPath(path);

    long num = userData.toLongLong();
    int fd = num;
    close(fd);
}

int VfsMac::readFileAtPath(QString path, QVariant userData, char *buffer, size_t size, off_t offset, QVariantMap &error)
{
    long num = userData.toLongLong();
    int fd = num;
    int ret = pread(fd, buffer, size, offset);
    if ( ret < 0 ) {
        error = errorWithPosixCode(errno);
        return -1;
    }
    return ret;
}

int VfsMac::writeFileAtPath(QString path, QVariant userData, const char *buffer, size_t size, off_t offset, QVariantMap &error)
{
    OCC::SyncWrapper::instance()->writeFileAtPath(path);

    long num = userData.toLongLong();
    int fd = num;
    int ret = pwrite(fd, buffer, size, offset);
    if ( ret < 0 ) {
        error = errorWithPosixCode(errno);
        return -1;
    }
    return ret;
}

bool VfsMac::preallocateFileAtPath(QString path, QVariant userData, int options, off_t offset, off_t length, QVariantMap &error)
{
    long num = userData.toLongLong();
    int fd = num;
    
    fstore_t fstore;
    
    fstore.fst_flags = 0;
    if ( options & ALLOCATECONTIG ) {
        fstore.fst_flags |= F_ALLOCATECONTIG;
    }
    if ( options & ALLOCATEALL ) {
        fstore.fst_flags |= F_ALLOCATEALL;
    }
    
    if ( options & ALLOCATEFROMPEOF ) {
        fstore.fst_posmode = F_PEOFPOSMODE;
    } else if ( options & ALLOCATEFROMVOL ) {
        fstore.fst_posmode = F_VOLPOSMODE;
    }
    
    fstore.fst_offset = offset;
    fstore.fst_length = length;
    
    if ( fcntl(fd, F_PREALLOCATE, &fstore) == -1 ) {
        error = errorWithPosixCode(errno);
        return false;
    }
    return true;
}

bool VfsMac::exchangeDataOfItemAtPath(QString path1, QString path2, QVariantMap &error)
{
    QString p1 = rootPath_ + path1;
    QString p2 = rootPath_ + path2;
    int ret = exchangedata(p1.toLatin1().data(), p2.toLatin1().data(), 0);
    if ( ret < 0 ) {
        error = errorWithPosixCode(errno);
        return false;
    }
    return true;
}

#pragma mark Getting and Setting Attributes

QVariantMap VfsMac::attributesOfFileSystemForPath(QString path, QVariantMap &error)
{
    QVariantMap attributes;
    
    unsigned long long defaultSize =(2LL * 1024 * 1024 * 1024);
    attributes.insert(FileManager::FMFileSystemSize, defaultSize);
    attributes.insert(FileManager::FMFileSystemFreeSize, defaultSize);
    attributes.insert(FileManager::FMFileSystemNodes, defaultSize);
    attributes.insert(FileManager::FMFileSystemFreeNodes, defaultSize);
    attributes.insert(kGMUserFileSystemVolumeMaxFilenameLengthKey, (int)255);
    attributes.insert(kGMUserFileSystemVolumeFileSystemBlockSizeKey, (int)4096);
    
    bool supports = true;
    
    attributes.insert(kGMUserFileSystemVolumeSupportsExchangeDataKey, supports);
    attributes.insert(kGMUserFileSystemVolumeSupportsAllocateKey, supports);
    
    FileManager fm;
    QVariantMap customAttribs = fm.attributesOfFileSystemForPath(rootPath_ + path, error);
    //qDebug() << "Path: " << rootPath_ + path;
    if(customAttribs.isEmpty())
    {
        if(error.empty())
            error = errorWithCode(ENODEV);
  //      qDebug() << " Error: " << error.values();
        attributes.clear();
        return attributes;
    }
    
    for(auto attrib : customAttribs.keys())
    {
 //       qDebug() << "Key: " <<attrib << "Value: " << customAttribs.value(attrib) << "\n";
        attributes.insert(attrib, customAttribs.value(attrib));
    }
    
    attributes.insert(FileManager::FMFileSystemSize, totalQuota());
    attributes.insert(FileManager::FMFileSystemFreeSize, totalQuota() - usedQuota());
    
    return attributes;
}

bool VfsMac::setAttributes(QVariantMap attributes, QString path, QVariant userInfo, QVariantMap &error)
{
    QString p = rootPath_ + path;
    
    // TODO: Handle other keys not handled by NSFileManager setAttributes call.
    
    long long offset = attributes.value(FileManager::FMFileSize).toLongLong();
    if ( attributes.contains(FileManager::FMFileSize) )
    {
        int ret = truncate(p.toLatin1().data(), offset);
        if ( ret < 0 )
        {
            error = errorWithPosixCode(errno);
            return false;
        }
    }
    int flags = attributes.value(kGMUserFileSystemFileFlagsKey).toInt();
    if (attributes.contains(kGMUserFileSystemFileFlagsKey))
    {
        int rc = chflags(p.toLatin1().data(), flags);
        if (rc < 0) {
            error = errorWithPosixCode(errno);
            return false;
        }
    }
    FileManager fm;
    return fm.setAttributes(attributes, p, error);
}

QVariantMap VfsMac::defaultAttributesOfItemAtPath(QString path, QVariant userData, QVariantMap &error)
{
    // Set up default item attributes.
    QVariantMap attributes;
    bool isReadOnly = internal_->isReadOnly();
   // qDebug() << "Path: " << rootPath_ + path;
    attributes.insert(FileManager::FMFilePosixPermissions, (isReadOnly ? 0555 : 0775));
    attributes.insert(FileManager::FMFileReferenceCount, (long long)1L);
    if (path == "/")
        attributes.insert(FileManager::FMFileType, FileManager::FMFileTypeDirectory);
    else
        attributes.insert(FileManager::FMFileType, FileManager::FMFileTypeRegular);
    
    QString p = rootPath_ + path;
    FileManager fm;
    QVariantMap *customAttribs = fm.attributesOfItemAtPath(p, error);
//    [[NSFileManager defaultManager] attributesOfItemAtPath:p error:error];
    
    // Maybe this is the root directory?  If so, we'll claim it always exists.
    if ((!customAttribs || customAttribs->empty()) && path=="/") {
        return attributes;  // The root directory always exists.
    }
    
    if (customAttribs && !customAttribs->empty())
    {
        for(auto attrib : customAttribs->keys())
        {
  /*          if (path == "/") {
                qDebug() << "Key: " <<attrib << "Value: " << customAttribs->value(attrib) << "\n";
            }*/
            attributes.insert(attrib, customAttribs->value(attrib));// addEntriesFromDictionary:customAttribs];
        }
        
    }
    else
    {
        if(error.empty())
            error = errorWithCode(ENOENT);
        return QVariantMap();
    }
    
    // If they don't supply a size and it is a file then we try to compute it.
   // qDebug() << attributes << "Si llego al final\n";
    return attributes;
}

QVariantMap* VfsMac::extendedTimesOfItemAtPath(QString path, QVariant userData, QVariantMap &error)
{
    FileManager fm;
    return fm.attributesOfItemAtPath(path, error);
}

#pragma mark Extended Attributes

QStringList* VfsMac::extendedAttributesOfItemAtPath(QString path, QVariantMap &error)
{
    QString p = rootPath_ + path;
    QStringList *retval = nullptr;
    
    ssize_t size = listxattr(p.toLatin1().data(), nullptr, 0, XATTR_NOFOLLOW);
    if ( size < 0 ) {
        error = errorWithPosixCode(errno);
        return retval;
    }
    char *data = new char[size];
    size = listxattr(p.toLatin1().data(), data, size, XATTR_NOFOLLOW);
    if ( size < 0 ) {
        error = errorWithPosixCode(errno);
        return retval;
    }
    char* ptr = data;
    QString s;
    retval = new QStringList();
    while ( ptr < (data + size) ) {
        s = QString(ptr);
        retval->append(s);
        ptr += (s.length() + 1);
    }
    return retval;
}

QByteArray* VfsMac::valueOfExtendedAttribute(QString name, QString path, off_t position, QVariantMap &error)
{
    QByteArray *data=nullptr;
    QString p = rootPath_ + path;
    
    ssize_t size = getxattr(p.toLatin1().data(), name.toLatin1().data(), nullptr, 0,
                            position, XATTR_NOFOLLOW);
    if ( size < 0 ) {
        error = errorWithPosixCode(errno);
        return data;
    }
    char* cdata = new char[size];
    size = getxattr(p.toLatin1().data(), name.toLatin1().data(), cdata, size,
                    position, XATTR_NOFOLLOW);
    
    if ( size < 0 )
    {
        error = errorWithPosixCode(errno);
        return data;
    }
    data = new QByteArray(cdata, size);
//    data.setRawData(cdata, size);
    return data;
}
bool VfsMac::setExtendedAttribute(QString name, QString path, QByteArray value, off_t position, int options, QVariantMap &error)
{
    // Setting com.apple.FinderInfo happens in the kernel, so security related
    // bits are set in the options. We need to explicitly remove them or the call
    // to setxattr will fail.
    // TODO: Why is this necessary?
    options &= ~(XATTR_NOSECURITY | XATTR_NODEFAULT);
    QString p = rootPath_ + path;
    int ret = setxattr(p.toLatin1().data(), name.toLatin1().data(),
                       value.data(), value.length(),
                       position, options | XATTR_NOFOLLOW);
    if ( ret < 0 ) {
        error = errorWithPosixCode(errno);
        return false;
    }
    return true;
}

bool VfsMac::removeExtendedAttribute(QString name, QString path, QVariantMap &error)
{
    QString p = rootPath_ + path;
    int ret = removexattr(p.toLatin1().data(), name.toLatin1().data(), XATTR_NOFOLLOW);
    if ( ret < 0 ) {
        error = errorWithPosixCode(errno);
        return false;
    }
    return true;
}

#pragma mark FUSE Operations

#define SET_CAPABILITY(conn, flag, enable)                                \
do {                                                                    \
if (enable) {                                                         \
(conn)->want |= (flag);                                             \
} else {                                                              \
(conn)->want &= ~(flag);                                            \
}                                                                     \
} while (0)

#define MAYBE_USE_ERROR(var, error)                                       \
//qDebug() << error;                                               \
if (!error.empty() && error.value("domain").toString() == FileManager::FMPOSIXErrorDomain) {            \
int code = error.value("code").toInt();                                            \
if (code != 0) {                                                      \
(var) = -code;                                                      \
}                                                                     \
}

static void* fusefm_init(struct fuse_conn_info* conn)
{
    VfsMac* fs = VfsMac::currentFS();
    try
    {
        fs->fuseInit();
    }
    catch (QException exception) { }
    
    SET_CAPABILITY(conn, FUSE_CAP_ALLOCATE, fs->enableAllocate());
    SET_CAPABILITY(conn, FUSE_CAP_XTIMES, fs->enableExtendedTimes());
    SET_CAPABILITY(conn, FUSE_CAP_VOL_RENAME, fs->enableSetVolumeName());
    SET_CAPABILITY(conn, FUSE_CAP_CASE_INSENSITIVE, !fs->enableCaseSensitiveNames());
    SET_CAPABILITY(conn, FUSE_CAP_EXCHANGE_DATA, fs->enableExchangeData());
    
    return fs;
}

static void fusefm_destroy(void* private_data)
{
    VfsMac* fs = (VfsMac *)private_data;
    try
    {
        fs->fuseDestroy();
      //  fs->deleteLater();
    }
    catch (QException exception) { }
}

static int fusefm_mkdir(const char* path, mode_t mode)
{
    int ret = -EACCES;
    
    try {
        QVariantMap error;
        unsigned long perm = mode & ALLPERMS;
        QVariantMap attribs;
        attribs.insert(FileManager::FMFilePosixPermissions, (long long)perm);
        VfsMac* fs = VfsMac::currentFS();
        if (fs->createDirectoryAtPath(QString::fromLatin1(path), attribs, error))
            ret = 0;  // Success!
        else
            if (!error.isEmpty())
                ret = -error.value("code").toInt();
       // qDebug() << "fusefm_mkdir" << attribs.keys() << attribs.values();
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
    int ret = -EACCES;
    
    try
    {
        QVariantMap error;
        QVariant userData;
        unsigned long perms = mode & ALLPERMS;
        QVariantMap attribs;
        attribs.insert(FileManager::FMFilePosixPermissions, (long long)perms);
        VfsMac* fs = VfsMac::currentFS();
        if (fs->createFileAtPath(path, attribs, fi->flags, userData, error))
        {
            ret = 0;
            if (userData.isValid())
                fi->fh = (uintptr_t)userData.toUInt();
        } else {
            MAYBE_USE_ERROR(ret, error);
        }
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_rmdir(const char* path)
{
    int ret = -EACCES;
    
    try
    {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->removeDirectoryAtPath(QString::fromLatin1(path), error))
            ret = 0;  // Success!
        else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_unlink(const char* path)
{
    int ret = -EACCES;
    try
    {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->removeItemAtPath(QString::fromLatin1(path),error))
            ret = 0;  // Success!
        else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_rename(const char* path, const char* toPath)
{
    int ret = -EACCES;
    
    try
    {
        QString source = QString::fromLatin1(path);
        QString destination = QString::fromLatin1(toPath);
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->moveItemAtPath(source, destination, error))
            ret = 0;  // Success!
        else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;  
}

static int fusefm_link(const char* path1, const char* path2)
{
    int ret = -EACCES;
    
    try
    {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->linkItemAtPath(QString::fromLatin1(path1), QString::fromLatin1(path2), error))
            ret = 0;  // Success!
        else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_symlink(const char* path1, const char* path2)
{
    int ret = -EACCES;
    
    try
    {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->createSymbolicLinkAtPath(QString::fromLatin1(path2), QString::fromLatin1(path1), error))
            ret = 0;  // Success!
        else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_readlink(const char *path, char *buf, size_t size)
{
    int ret = -ENOENT;
    
    try
    {
        QString linkPath = QString::fromLatin1(path);
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        QString pathContent = fs->destinationOfSymbolicLinkAtPath(linkPath, error);
        if (!pathContent.isEmpty())
        {
            ret = 0;
            QFileInfo fi(pathContent);
            buf = fi.absoluteFilePath().toLatin1().data();
        } else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info* fi)
{
    QVariantMap error;
    int ret = -ENOENT;
    
    try
    {
        VfsMac* fs = VfsMac::currentFS();
        QStringList *contents = fs->contentsOfDirectoryAtPath(QString::fromLatin1(path), error);
        if (contents)
        {
            ret = 0;
            for (int i = 0, count = contents->length(); i < count; i++)
                filler(buf, contents->at(i).toLatin1().data(), NULL, 0);
        }
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_open(const char *path, struct fuse_file_info* fi) {
    int ret = -ENOENT;  // TODO: Default to 0 (success) since a file-system does
    // not necessarily need to implement open?
    
    try
    {
        QVariant userData;
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();

        if (fs->openFileAtPath(QString::fromLatin1(path), fi->flags, userData, error))
        {
            ret = 0;
            if (userData.isValid())
                fi->fh = (uintptr_t)userData.toUInt();
        }
        else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { } return ret;
}

static int fusefm_release(const char *path, struct fuse_file_info* fi)
{
    try
    {
        QVariant userData = fi->fh;
        VfsMac* fs = VfsMac::currentFS();
        fs->releaseFileAtPath(QString::fromLatin1(path), userData);
    }
    catch (QException exception) { }
    return 0;
}

static int fusefm_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info* fi)
{
    int ret = -EIO;
    
    try
    {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        ret = fs->readFileAtPath(QString::fromLatin1(path), fi->fh, buf, size, offset, error);
        MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_write(const char* path, const char* buf, size_t size,
                        off_t offset, struct fuse_file_info* fi)
{
    int ret = -EIO;
    
    try {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        ret = fs->writeFileAtPath(QString::fromLatin1(path), fi->fh, buf, size, offset, error);
        MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_fsync(const char* path, int isdatasync,
                        struct fuse_file_info* fi) {
    // TODO: Support fsync?
    return 0;
}

static int fusefm_fallocate(const char* path, int mode, off_t offset, off_t length,
                            struct fuse_file_info* fi)
{
    int ret = -ENOSYS;
    try
    {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->preallocateFileAtPath(QString::fromLatin1(path), (fi ? fi->fh : QVariant()), mode, offset, length, error))
        {
            ret = 0;
        } else {
            MAYBE_USE_ERROR(ret, error);
        }
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_exchange(const char* p1, const char* p2, unsigned long opts) {
    int ret = -ENOSYS;
    try {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->exchangeDataOfItemAtPath(QString::fromLatin1(p1), QString::fromLatin1(p2), error)) {
            ret = 0;
        } else {
            MAYBE_USE_ERROR(ret, error);
        }
    }
    catch (QException exception) { }
    return ret;  
}

static int fusefm_statfs_x(const char* path, struct statfs* stbuf)
{
    int ret = -ENOENT;
    //qDebug() << "Path: " << path << "QString: " << QString::fromLatin1(path);
    try {
        memset(stbuf, 0, sizeof(struct statfs));
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->fillStatfsBuffer(stbuf, QString::fromLatin1(path), error))
            ret = 0;
        else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_setvolname(const char* name)
{
    int ret = -ENOSYS;
    try
    {
        QVariantMap error;
        QVariantMap attribs;
        attribs.insert(kGMUserFileSystemVolumeNameKey, QString::fromLatin1(name));
        VfsMac* fs = VfsMac::currentFS();
        if (fs->setAttributes(attribs, "/", QVariantMap(), error)) {
            ret = 0;
        } else {
            MAYBE_USE_ERROR(ret, error);
        }
    }
    catch (QException exception) { }
    return ret;
}


static int fusefm_fgetattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info* fi)
{
    int ret = -ENOENT;
    //qDebug() << "Path: " << path << "QString: " << QString::fromLatin1(path);
    try {
        memset(stbuf, 0, sizeof(struct stat));
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        QVariant userData = fi ? fi->fh : 0;
        if (fs->fillStatBuffer(stbuf, QString::fromLatin1(path), userData, error))
            ret = 0;
        else {
            MAYBE_USE_ERROR(ret, error);
           // qDebug() << error;
        }
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_getattr(const char *path, struct stat *stbuf) {
    return fusefm_fgetattr(path, stbuf, nullptr);
   // qDebug() << "Path: " << path << "QString: " << QString::fromLatin1(path);
}

static int fusefm_getxtimes(const char* path, struct timespec* bkuptime,
                            struct timespec* crtime)
{
    int ret = -ENOENT;
    
    try
    {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        QVariantMap *attribs = fs->extendedTimesOfItemAtPath(QString::fromLatin1(path), QVariant(), error);
        if (attribs) {
            ret = 0;
            QDateTime creationDate = attribs->value(FileManager::FMFileCreationDate).toDateTime();
            if (creationDate.isValid())
            {
                const double seconds_dp = creationDate.toMSecsSinceEpoch()/1000;
                const time_t t_sec = (time_t) seconds_dp;
                const double nanoseconds_dp = ((seconds_dp - t_sec) * kNanoSecondsPerSecond);
                const long t_nsec = (nanoseconds_dp > 0 ) ? nanoseconds_dp : 0;
                crtime->tv_sec = t_sec;
                crtime->tv_nsec = t_nsec;
            } else {
                memset(crtime, 0, sizeof(struct timespec));
            }
            QDateTime backupDate = attribs->value(kGMUserFileSystemFileBackupDateKey).toDateTime();
            if (backupDate.isValid()) {
                const double seconds_dp = backupDate.toMSecsSinceEpoch()/1000;
                const time_t t_sec = (time_t) seconds_dp;
                const double nanoseconds_dp = ((seconds_dp - t_sec) * kNanoSecondsPerSecond);
                const long t_nsec = (nanoseconds_dp > 0 ) ? nanoseconds_dp : 0;
                bkuptime->tv_sec = t_sec;
                bkuptime->tv_nsec = t_nsec;
            } else {
                memset(bkuptime, 0, sizeof(struct timespec));
            }
        } else {
            MAYBE_USE_ERROR(ret, error);
        }
    }
    catch (QException exception) { }
    return ret;
}

static QDateTime dateWithTimespec(const struct timespec* spec)
{
    unsigned long long time_ns = spec->tv_nsec;
    unsigned long long time_sec = spec->tv_sec + (time_ns / kNanoSecondsPerSecond);
    return QDateTime::fromMSecsSinceEpoch(time_sec*1000);
}

static QVariantMap dictionaryWithAttributes(const struct setattr_x* attrs)
{
    QVariantMap dict;
    if (SETATTR_WANTS_MODE(attrs))
    {
        unsigned long long perm = attrs->mode & ALLPERMS;
        dict.insert(FileManager::FMFilePosixPermissions, perm);
    }
    if (SETATTR_WANTS_UID(attrs))
        dict.insert(FileManager::FMFileOwnerAccountID, attrs->uid);
    if (SETATTR_WANTS_GID(attrs))
        dict.insert(FileManager::FMFileGroupOwnerAccountID, attrs->gid);
    if (SETATTR_WANTS_SIZE(attrs))
        dict.insert(FileManager::FMFileSize, attrs->size);
    if (SETATTR_WANTS_ACCTIME(attrs))
        dict.insert(kGMUserFileSystemFileAccessDateKey, dateWithTimespec(&(attrs->acctime)));
    if (SETATTR_WANTS_MODTIME(attrs))
        dict.insert(FileManager::FMFileModificationDate, dateWithTimespec(&(attrs->modtime)));
    if (SETATTR_WANTS_CRTIME(attrs))
        dict.insert(FileManager::FMFileCreationDate, dateWithTimespec(&(attrs->crtime)));
    if (SETATTR_WANTS_CHGTIME(attrs))
        dict.insert(kGMUserFileSystemFileChangeDateKey, dateWithTimespec(&(attrs->chgtime)));
    if (SETATTR_WANTS_BKUPTIME(attrs))
        dict.insert(kGMUserFileSystemFileBackupDateKey, dateWithTimespec(&(attrs->bkuptime)));
    if (SETATTR_WANTS_FLAGS(attrs))
        dict.insert(kGMUserFileSystemFileFlagsKey, attrs->flags);
    return dict;
}

static int fusefm_fsetattr_x(const char* path, struct setattr_x* attrs,
                             struct fuse_file_info* fi)
{
    int ret = 0;  // Note: Return success by default.
    
    try
    {
        QVariantMap error;
        QVariantMap attribs = dictionaryWithAttributes(attrs);
        VfsMac* fs = VfsMac::currentFS();
        if (fs->setAttributes(attribs, QString::fromLatin1(path), (fi ? fi->fh : QVariant()), error))
            ret = 0;
        else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_setattr_x(const char* path, struct setattr_x* attrs) {
    return fusefm_fsetattr_x(path, attrs, nullptr);
}

static int fusefm_listxattr(const char *path, char *list, size_t size)
{
    int ret = -ENOTSUP;
    try
    {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        QStringList *attributeNames = fs->extendedAttributesOfItemAtPath(QString::fromLatin1(path), error);
        if (attributeNames) {
            char zero = 0;
            QByteArray data;
            for (int i = 0, count = attributeNames->length(); i < count; i++)
            {
                data.append(attributeNames->at(i));
                data.append(zero);
            }
            ret = data.length();  // default to returning size of buffer.
            if (list)
            {
                if (size > (unsigned long)data.length())
                    size = data.length();
                memcpy(list, data.data(), size);
                //[data getBytes:list length:size];
            }
        } else
        {
            MAYBE_USE_ERROR(ret, error);
        }
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_getxattr(const char *path, const char *name, char *value,
                           size_t size, uint32_t position)
{
    int ret = -ENOATTR;
   // qDebug() << "Path: " << path << "QString: " << QString::fromLatin1(path);
    try {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        QByteArray* data = fs->valueOfExtendedAttribute(QString::fromLatin1(name), QString::fromLatin1(path), position, error);
        if (data) {
            ret = data->length();  // default to returning size of buffer.
            if (value) {
                if (size > (unsigned long)data->length()) {
                    size = data->length();
                }
                memcpy(value, data->data(), size);
                ret = size;  // bytes read
            }
        } else {
            MAYBE_USE_ERROR(ret, error);
        }
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_setxattr(const char *path, const char *name, const char *value,
                           size_t size, int flags, uint32_t position)
{
    int ret = -EPERM;
    try {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->setExtendedAttribute(QString::fromLatin1(name), QString::fromLatin1(path), QByteArray(value, size), position, flags, error))
            ret = 0;
        else {
            MAYBE_USE_ERROR(ret, error);
        }
    }
    catch (QException exception) { }
    return ret;
}

static int fusefm_removexattr(const char *path, const char *name)
{
    int ret = -ENOATTR;
    try
    {
        QVariantMap error;
        VfsMac* fs = VfsMac::currentFS();
        if (fs->removeExtendedAttribute(QString(name), QString(path), error))
            ret = 0;
        else
            MAYBE_USE_ERROR(ret, error);
    }
    catch (QException exception) { }
    return ret;
}

#undef MAYBE_USE_ERROR

static struct fuse_operations fusefm_oper = {
    .init = fusefm_init,
    .destroy = fusefm_destroy,
    
    // Creating an Item
    .mkdir = fusefm_mkdir,
    .create = fusefm_create,
    
    // Removing an Item
    .rmdir = fusefm_rmdir,
    .unlink = fusefm_unlink,
    
    // Moving an Item
    .rename = fusefm_rename,
    
    // Linking an Item
    .link = fusefm_link,
    
    // Symbolic Links
    .symlink = fusefm_symlink,
    .readlink = fusefm_readlink,
    
    // Directory Contents
    .readdir = fusefm_readdir,
    
    // File Contents
    .open	= fusefm_open,
    .release = fusefm_release,
    .read	= fusefm_read,
    .write = fusefm_write,
    .fsync = fusefm_fsync,
    .fallocate = fusefm_fallocate,
    .exchange = fusefm_exchange,
    
    // Getting and Setting Attributes
    .statfs_x = fusefm_statfs_x,
    .setvolname = fusefm_setvolname,
    .getattr = fusefm_getattr,
    .fgetattr = fusefm_fgetattr,
    .getxtimes = fusefm_getxtimes,
    .setattr_x = fusefm_setattr_x,
    .fsetattr_x = fusefm_fsetattr_x,
    
    // Extended Attributes
    .listxattr = fusefm_listxattr,
    .getxattr = fusefm_getxattr,
    .setxattr = fusefm_setxattr,
    .removexattr = fusefm_removexattr,
};

/*VfsMac::~VfsMac()
{
    internal_->deleteLater();
}*/

#pragma mark Internal Mount

void VfsMac::mount(QVariantMap args)
{
    Q_ASSERT(internal_->status() == GMUserFileSystem_NOT_MOUNTED);
    internal_->setStatus(GMUserFileSystem_MOUNTING);
    
    QStringList options = args.value("options").toStringList();
    bool isThreadSafe = internal_->isThreadSafe();
    bool shouldForeground = args.value("shouldForeground").toBool();
    
    // Maybe there is a dead FUSE file system stuck on our mount point?
    struct statfs statfs_buf;
    memset(&statfs_buf, 0, sizeof(statfs_buf));
    int ret = statfs(internal_->mountPath().toLatin1().data(), &statfs_buf);
    if (ret == 0)
    {
        if (statfs_buf.f_fssubtype == (unsigned int)(-1))
        {
            // We use a special indicator value from FUSE in the f_fssubtype field to
            // indicate that the currently mounted filesystem is dead. It probably
            // crashed and was never unmounted.
            ret = ::unmount(internal_->mountPath().toLatin1().data(), 0);
            if (ret != 0)
            {
                QVariantMap userData = errorWithCode(errno);
                QString description = userData.value("localizedDescription").toString() + " " + tr("Unable to unmount an existing 'dead' filesystem.");
                userData.insert("localizedDescription", description);
                emit FuseFileSystemMountFailed(userData);
                return;
            }
            if (internal_->mountPath().startsWith("/Volumes/"))
            {
                // Directories for mounts in @"/Volumes/..." are removed automatically
                // when an unmount occurs. This is an asynchronous process, so we need
                // to wait until the directory is removed before proceeding. Otherwise,
                // it may be removed after we try to create the mount directory and the
                // mount attempt will fail.
                bool isDirectoryRemoved = false;
                static const int kWaitForDeadFSTimeoutSeconds = 5;
                struct stat stat_buf;
                for (int i = 0; i < 2 * kWaitForDeadFSTimeoutSeconds; ++i)
                {
                    usleep(500000);  // .5 seconds
                    ret = stat(internal_->mountPath().toLatin1().data(), &stat_buf);
                    if (ret != 0 && errno == ENOENT)
                    {
                        isDirectoryRemoved = true;
                        break;
                    }
                }
                if (!isDirectoryRemoved) {
                    QString description = tr("Gave up waiting for directory under /Volumes to be removed after "
                                             "cleaning up a dead file system mount.");
                    QVariantMap userData = errorWithCode(GMUserFileSystem_ERROR_UNMOUNT_DEADFS_RMDIR);
                    userData.insert("localizedDescription", description);
                    emit FuseFileSystemMountFailed(userData);
                    return;
                }
            }
        }
    }
    
    // Check mount path as necessary.
    struct stat stat_buf;
    memset(&stat_buf, 0, sizeof(stat_buf));
    ret = stat(internal_->mountPath().toLatin1().data(), &stat_buf);
    if ((ret == 0 && !S_ISDIR(stat_buf.st_mode)) ||
        (ret != 0 && errno == ENOTDIR))
    {
        emit FuseFileSystemMountFailed(errorWithCode(ENOTDIR));
        return;
    }
    
    // Trigger initialization of NSFileManager. This is rather lame, but if we
    // don't call directoryContents before we mount our FUSE filesystem and
    // the filesystem uses NSFileManager we may deadlock. It seems that the
    // NSFileManager class will do lazy init and will query all mounted
    // filesystems. This leads to deadlock when we re-enter our mounted FUSE file
    // system. Once initialized it seems to work fine.
    QDir dir("/Volumes");
    dir.entryList();
    
    QStringList arguments;
    arguments.append(QCoreApplication::applicationFilePath());
    
    if (!isThreadSafe)
        arguments.append("-s");
    if (shouldForeground)
        arguments.append("-f"); // Foreground rather than daemonize.
    for (int i = 0; i < options.length(); ++i)
    {
        QString option = options.at(i);
        if (!option.isEmpty())
            arguments.append(QString("-o") + option);
    }
    arguments.append(internal_->mountPath());
    
    // Start Fuse Main
    int argc = arguments.length();
    char** argv = new char*[argc];
    for (int i = 0, count = argc; i < count; i++)
    {
        QString argument = arguments.at(i);
        argv[i] = strdup(argument.toLatin1().data());  // We'll just leak this for now.
    }
    ret = fuse_main(argc, (char **)argv, &fusefm_oper, this);
    
    if (internal_.data()!=nullptr && internal_->status() == GMUserFileSystem_MOUNTING) {
        // If we returned from fuse_main while we still think we are
        // mounting then an error must have occurred during mount.
        QString description = QString("Internal FUSE error (rc=%1) while attempting to mount the file system. "
                                 "For now, the best way to diagnose is to look for error messages using "
                                 "Console.").arg(errno);
        QVariantMap userData = errorWithCode(errno);
        userData.insert("localizedDescription", QVariant(userData.value("localizedDescription").toString() + description));
        emit FuseFileSystemMountFailed(userData);
    } else if (internal_.data()!=nullptr)
        internal_->setStatus(GMUserFileSystem_NOT_MOUNTED);
}
















