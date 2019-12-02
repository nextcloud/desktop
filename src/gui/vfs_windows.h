/*
* Copyright(C) 2018 by AMCO
* Copyright(C) 2018 by Jonathan Ponciano <jponcianovera@ciencias.unam.mx>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
*(at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE.See the GNU General Public License
* for more details.
*/

#ifndef VFS_WINDOWS_H
#define VFS_WINDOWS_H

#include "../../dokanLib/dokan.h"
#include "../../dokanLib/fileinfo.h"

#include "discoveryphase.h"
#include "accountstate.h"
#include "configfile.h"

#include <QMutex>
#include <QWaitCondition>
#include <QRunnable>
#include <QThreadPool>
#include <QStorageInfo>
#include "syncwrapper.h"

namespace OCC {

class CleanIgnoredTask : public QObject, public QRunnable
{
	Q_OBJECT
public:
	void run();
};

class VfsWindows : public QObject
{
    Q_OBJECT
public:
    ~VfsWindows();
    static VfsWindows *instance();
    void initialize(QString rootPath, WCHAR mountLetter, AccountState *accountState);
    void mount();
    void unmount();
    bool removeRecursively(const QString &dirName);
    bool removeDir();

	void setNumberOfBytes(unsigned long long numberOfBytes);
    unsigned long long getNumberOfBytes();

    void setNumberOfFreeBytes(unsigned long long numberOfFreeBytes);
    unsigned long long getNumberOfFreeBytes();

	QStringList* contentsOfDirectoryAtPath(QString path, QVariantMap &error);
	QList<QString> ignoredList;

	void createFileAtPath(QString path, QVariantMap &error);
	void moveFileAtPath(QString path, QString npath,QVariantMap &error);
	void createDirectoryAtPath(QString path, QVariantMap &error);
	void moveDirectoryAtPath(QString path, QString npath, QVariantMap &error);

	void openFileAtPath(QString path, QVariantMap &error);
	void writeFileAtPath(QString path, QVariantMap &error);
	void deleteFileAtPath(QString path, QVariantMap &error);
	void startDeleteDirectoryAtPath(QString path, QVariantMap &error);
	void endDeleteDirectoryAtPath(QString path, QVariantMap &error);

	QString getAvailableLogicalDrive();

private:
	VfsWindows();
    static VfsWindows *_instance;
    QMap<QString, OCC::DiscoveryDirectoryResult *> _fileListMap;
    QPointer<OCC::DiscoveryFolderFileList> _remotefileListJob;
    QString rootPath;
    WCHAR mountLetter;

	// @Capacity
    //*TotalNumberOfBytes = (ULONGLONG)1024L * 1024 * 1024 * 50;
    unsigned long long numberOfBytes = 0;
    // @Used space
    //*TotalNumberOfFreeBytes = (ULONGLONG)1024L * 1024 * 10;
    unsigned long long numberOfFreeBytes = 0;
    // @Free space
    //*FreeBytesAvailable = (ULONGLONG)(*TotalNumberOfBytes - *TotalNumberOfFreeBytes); / *1024 * 1024 * 10;
    unsigned long long freeBytesAvailable = 0;

	// To sync
	OCC::SyncWrapper *_syncWrapper;
    QMutex _mutex;
    QWaitCondition _syncCondition;
    QWaitCondition _dirCondition;

signals:
	void startRemoteFileListJob(QString path);

	// To sync: propagate FUSE operations to the sync engine
    void addToFileTree(const QString path);
    void createFile(const QString path);
    void deleteFile(const QString path);
    void moveFile(const QString path);
    void openFile(const QString path);
    void releaseFile(const QString path);
    void writeFile(const QString path);
    
	void createDirectory(const QString path);
    void moveDirectory(const QString path);

public slots:
	void folderFileListFinish(OCC::DiscoveryDirectoryResult *dr);

	// To sync: notify syncing is done
    void slotSyncFinish(const QString &path, bool status);
};

} // namespace OCC

#endif // VFS_WINDOWS_H